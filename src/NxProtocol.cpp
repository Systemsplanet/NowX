#include "NxProtocol.h"
#include "Log.h"
#include <string.h>
#include <stdio.h>

namespace nowx {

// ========================================================================
// Message
// ========================================================================

Message::Message(Message&& o) noexcept
  : _owner(o._owner), _slot(o._slot), _ptr(o._ptr),
    _len(o._len), _flags(o._flags), _msg(o._msg)
{
  o._owner = nullptr;
  o._slot  = -1;
  o._ptr   = nullptr;
  o._len   = 0;
}

Message& Message::operator=(Message&& o) noexcept {
  if (this != &o) {
    release();
    _owner = o._owner; _slot  = o._slot;
    _ptr   = o._ptr;   _len   = o._len;
    _flags = o._flags; _msg   = o._msg;
    o._owner = nullptr; o._slot = -1;
    o._ptr   = nullptr; o._len  = 0;
  }
  return *this;
}

std::string Message::str() const {
  if (!_ptr || _len == 0) return {};
  return std::string(reinterpret_cast<const char*>(_ptr), _len);
}

void Message::release() {
  if (_owner && _slot >= 0) {
    _owner->_releaseSlot(_slot);
    _owner = nullptr;
    _slot  = -1;
    _ptr   = nullptr;
    _len   = 0;
  }
}

void Message::_bind(NxProtocol *p, uint8_t *ptr, uint32_t len,
                    uint8_t flags, uint32_t msg, int slot) {
  _owner = p;
  _ptr   = ptr;
  _len   = len;
  _flags = flags;
  _msg   = msg;
  _slot  = slot;
}

// ========================================================================
// NxProtocol — construction & config
// ========================================================================

NxProtocol::NxProtocol(ITransport *t) : _t(t) {
  if (_t) {
    _t->onRecv([this](const uint8_t mac[6], const uint8_t *d, size_t n) {
      this->_onPacket(mac, d, n);
    });
  }
}

void NxProtocol::setPeer(const uint8_t mac[6]) {
  memcpy(_tx.peer, mac, 6);
  _tx.peerSet = true;
}

bool NxProtocol::setPeerStr(const char *s) {
  uint8_t m[6];
  if (!parseMac(s, m)) return false;
  setPeer(m);
  return true;
}

void NxProtocol::setKey(const uint8_t *key, uint32_t len) {
  memset(_key, 0, sizeof(_key));
  if (!key || len == 0) { _obf = false; return; }
  uint32_t n = len < 32 ? len : 32;
  memcpy(_key, key, n);
  _obf = true;
}

// ========================================================================
// Pure helpers
// ========================================================================

uint32_t NxProtocol::crc32(const uint8_t *d, uint32_t len) {
  if (!d || len == 0) return 0;
  uint32_t c = 0xFFFFFFFFu;
  for (uint32_t i = 0; i < len; i++) {
    c ^= d[i];
    for (uint8_t j = 0; j < 8; j++) {
      uint32_t mask = (uint32_t)-(int32_t)(c & 1u);
      c = (c >> 1) ^ (0xEDB88320u & mask);
    }
  }
  return ~c;
}

bool NxProtocol::parseMac(const char *s, uint8_t mac[6]) {
  if (!s) return false;
  unsigned v[6];
  int n = sscanf(s, "%x:%x:%x:%x:%x:%x",
                 &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]);
  if (n != 6) return false;
  for (int i = 0; i < 6; i++) {
    if (v[i] > 0xFF) return false;
    mac[i] = (uint8_t)v[i];
  }
  return true;
}

void NxProtocol::xorInPlace(uint8_t *d, uint32_t len) const {
  if (!_obf) return;
  for (uint32_t i = 0; i < len; i++) d[i] ^= _key[i & 31];
}

// ========================================================================
// Pool & slot management
// ========================================================================

NxProtocol::Frag *NxProtocol::_allocFrag() {
  for (auto &f : _pool) {
    if (!f.used) { f.used = true; return &f; }
  }
  return nullptr; // pool exhausted
}

void NxProtocol::_freeFrag(Frag *f) {
  if (f) f->used = false;
}

NxProtocol::RxAsm *NxProtocol::_asmGet(uint32_t msg, const uint8_t mac[6]) {
  _asmGc();
  // Return an existing slot for this (msg, src) pair.
  for (auto &a : _asm) {
    if (a.used && a.msg == msg && memcmp(a.srcMac, mac, 6) == 0) return &a;
  }
  // Allocate a new slot.
  for (auto &a : _asm) {
    if (!a.used) {
      memset(&a, 0, sizeof(a));
      a.used   = true;
      a.msg    = msg;
      memcpy(a.srcMac, mac, 6);
      a.lastMs = _t ? _t->millis() : 0;
      return &a;
    }
  }
  return nullptr; // all slots busy
}

void NxProtocol::_asmGc() {
  if (!_t) return;
  uint32_t now = _t->millis();
  for (auto &a : _asm) {
    if (!a.used) continue;
    if ((now - a.lastMs) <= NX_TTL) continue;
    // Free every non-null fragment pointer, regardless of a.cnt,
    // so a partially-initialised slot never leaks.
    for (uint16_t i = 0; i < NX_MAX_BLOCKS; i++) _freeFrag(a.frag[i]);
    memset(&a, 0, sizeof(a));
  }
}

int NxProtocol::_rxAlloc() {
  for (int i = 0; i < NX_RXQ; i++) {
    if (!_rxSlots[i].used) { _rxSlots[i].used = true; return i; }
  }
  return -1;
}

void NxProtocol::_releaseSlot(int slot) {
  if (slot < 0 || slot >= NX_RXQ) return;
  _rxSlots[slot].used = false;
  _rxSlots[slot].len  = 0;
}

// ========================================================================
// Dedup
// ========================================================================

bool NxProtocol::_isDup(const uint8_t mac[6], uint32_t msg) {
  for (auto &e : _dedup) {
    if (e.msg == msg && memcmp(e.mac, mac, 6) == 0) return true;
  }
  return false;
}

void NxProtocol::_markSeen(const uint8_t mac[6], uint32_t msg) {
  auto &e = _dedup[_dedupHead];
  memcpy(e.mac, mac, 6);
  e.msg      = msg;
  _dedupHead = (uint8_t)((_dedupHead + 1) % NX_DUP);
}

// ========================================================================
// Send path
// ========================================================================

bool NxProtocol::_sendRaw(const uint8_t *d, uint32_t len) {
  if (!_t || !_tx.peerSet) return false;
  return _t->send(_tx.peer, d, len);
}

bool NxProtocol::send(const std::string &s, uint8_t flags) {
  return send(reinterpret_cast<const uint8_t*>(s.data()),
              (uint32_t)s.size(), flags);
}

bool NxProtocol::send(const uint8_t *data, uint32_t len, uint8_t flags) {
  if (!_t || !_tx.peerSet) return false;
  if (len > NX_MAX_BYTES)  return false;

  uint32_t msg  = ++_tx.msgCtr;
  uint16_t cnt  = (uint16_t)((len + NX_PAY - 1) / NX_PAY);
  if (cnt == 0) cnt = 1;
  if (cnt > NX_MAX_BLOCKS) return false;

  uint8_t pkt[sizeof(NxHdr) + NX_PAY];

  for (uint16_t base = 0; base < cnt; base += NX_WIN) {
    uint16_t end = (uint16_t)(base + NX_WIN < cnt ? base + NX_WIN : cnt);

    // Build expected-ACK bitmap for this window.
    uint32_t expected = 0;
    for (uint16_t b = base; b < end; b++) expected |= (1UL << (b - base));

    bool ok = false;
    for (uint8_t r = 0; r < NX_RETRY && !ok; r++) {

      // Transmit every fragment in the window.
      for (uint16_t b = base; b < end; b++) {
        uint32_t off    = (uint32_t)b * NX_PAY;
        uint16_t sz     = (uint16_t)(len - off > NX_PAY ? NX_PAY : len - off);
        uint8_t *wirePay = pkt + sizeof(NxHdr);

        if (sz) memcpy(wirePay, data + off, sz);
        if (flags & NX_OBFUSCATE) xorInPlace(wirePay, sz);

        NxHdr h{};
        h.magic = NX_MAGIC;
        h.msg   = msg;
        h.idx   = b;
        h.cnt   = cnt;
        h.pay   = sz;
        h.flags = flags;
        h.crc   = crc32(wirePay, sz);
        memcpy(pkt, &h, sizeof(h));

        if (!_sendRaw(pkt, sizeof(h) + sz)) return false;
      }

      if (_waitAck(msg, base, expected)) {
        ok = true;
      } else if (r + 1 < NX_RETRY) {
        logf("nowx: retry win base=%u msg=%u attempt=%u",
             (unsigned)base, (unsigned)msg, (unsigned)(r + 1));
      }
    }
    if (!ok) return false;
  }
  return true;
}

bool NxProtocol::_waitAck(uint32_t msg, uint32_t base, uint32_t expected) {
  if (!_t) return false;
  uint32_t st = _t->millis();
  while ((_t->millis() - st) < NX_TIMEOUT_MS) {

    // Read _ackReady / _lastAck under the spinlock (on-device) or
    // directly (host — single-threaded tests).
    bool     ready = false;
    NxAck    snap{};
#ifndef NOWX_HOST_BUILD
    taskENTER_CRITICAL(&_ackLock);
#endif
    if (_ackReady) {
      snap  = _lastAck;
      ready = true;
    }
#ifndef NOWX_HOST_BUILD
    taskEXIT_CRITICAL(&_ackLock);
#endif

    if (ready &&
        snap.msg  == msg  &&
        snap.base == base &&
        (snap.ack & expected) == expected) {
      // Consume the ACK.
#ifndef NOWX_HOST_BUILD
      taskENTER_CRITICAL(&_ackLock);
#endif
      _ackReady = false;
#ifndef NOWX_HOST_BUILD
      taskEXIT_CRITICAL(&_ackLock);
#endif
      return true;
    }
    _t->yieldMs(1);
  }
  return false;
}

void NxProtocol::_sendAck(uint32_t msg, uint32_t base, uint32_t ackMap,
                           const uint8_t mac[6]) {
  NxAck a{};
  a.type = NX_IS_ACK;
  a.msg  = msg;
  a.base = base;
  a.ack  = ackMap;
  a.t    = _t ? _t->millis() : 0;
  if (_t) _t->send(mac, reinterpret_cast<uint8_t*>(&a), sizeof(a));
}

// ========================================================================
// Receive path
// ========================================================================

void NxProtocol::_onPacket(const uint8_t mac[6],
                            const uint8_t *d, size_t len) {
  if (!d || len == 0) return;
  // Distinguish ACK frames by the NX_IS_ACK type byte at offset 0,
  // not by packet length (length-based dispatch was fragile).
  if (len >= sizeof(NxAck) && d[0] == NX_IS_ACK) {
    _handleAck(d, len);
    return;
  }
  if (len >= sizeof(NxHdr)) _handleData(mac, d, len);
}

void NxProtocol::_handleAck(const uint8_t *d, size_t /*len*/) {
  NxAck a;
  memcpy(&a, d, sizeof(a));
#ifndef NOWX_HOST_BUILD
  taskENTER_CRITICAL(&_ackLock);
#endif
  _lastAck  = a;
  _ackReady = true;
#ifndef NOWX_HOST_BUILD
  taskEXIT_CRITICAL(&_ackLock);
#endif
}

void NxProtocol::_handleData(const uint8_t mac[6],
                              const uint8_t *d, size_t len) {
  NxHdr h;
  memcpy(&h, d, sizeof(h));

  // Validate header fields before touching any state.
  if (h.magic != NX_MAGIC)            return;
  if (h.cnt   == 0)                   return;
  if (h.cnt   >  NX_MAX_BLOCKS)       return;
  if (h.idx   >= h.cnt)               return;
  if (h.pay   >  NX_PAY)              return;
  if (len < sizeof(NxHdr) + h.pay)    return;

  const uint8_t *wirePay = d + sizeof(NxHdr);
  if (crc32(wirePay, h.pay) != h.crc) return;

  // If we've already delivered this message, re-send the full-window ACK
  // so the sender stops retrying.
  if (_isDup(mac, h.msg)) {
    uint32_t base = (h.idx / NX_WIN) * NX_WIN;
    uint16_t end  = (uint16_t)(base + NX_WIN < h.cnt ? base + NX_WIN : h.cnt);
    uint32_t map  = 0;
    for (uint16_t b = base; b < end; b++) map |= (1UL << (b - base));
    _sendAck(h.msg, base, map, mac);
    return;
  }

  RxAsm *a = _asmGet(h.msg, mac);
  if (!a) return;

  a->cnt    = h.cnt;
  a->lastMs = _t ? _t->millis() : 0;

  if (!a->frag[h.idx]) {
    Frag *f = _allocFrag();
    if (!f) return; // pool exhausted — fragment dropped
    memcpy(f->data, wirePay, h.pay);
    if (h.flags & NX_OBFUSCATE) xorInPlace(f->data, h.pay);
    f->len        = h.pay;
    a->frag[h.idx] = f;
    a->got++;
  }

  // Send per-window ACK based on what we have so far.
  uint32_t base = (h.idx / NX_WIN) * NX_WIN;
  uint16_t end  = (uint16_t)(base + NX_WIN < h.cnt ? base + NX_WIN : h.cnt);
  uint32_t map  = 0;
  for (uint16_t b = base; b < end; b++) {
    if (a->frag[b]) map |= (1UL << (b - base));
  }
  _sendAck(h.msg, base, map, mac);

  // Check for message completion.
  if (a->got != a->cnt) return;

  int slot = _rxAlloc();
  if (slot < 0) return; // RX queue full — message dropped

  RxMsg &r  = _rxSlots[slot];
  uint32_t pos = 0;
  for (uint16_t i = 0; i < a->cnt; i++) {
    Frag *f = a->frag[i];
    if (!f || pos + f->len > NX_MAX_BYTES) {
      _releaseSlot(slot);
      return;
    }
    memcpy(r.buf + pos, f->data, f->len);
    pos += f->len;
    _freeFrag(f);
  }
  r.len   = pos;
  r.flags = h.flags;
  r.msg   = h.msg;

  // Enqueue into the circular RX queue.
  uint8_t head = _rxH;
  uint8_t next = (uint8_t)((head + 1) & (NX_RXQ - 1));
  if (next == _rxT) {
    // Queue full — drop the message and release the slot.
    _releaseSlot(slot);
  } else {
    _rxQueue[head] = (uint8_t)slot;
    _rxH           = next;
    _markSeen(mac, h.msg);
  }
  memset(a, 0, sizeof(*a));
}

bool NxProtocol::receive(Message &out) {
  if (_rxT == _rxH) return false;
  uint8_t slot = _rxQueue[_rxT];
  _rxT = (uint8_t)((_rxT + 1) & (NX_RXQ - 1));
  RxMsg &r = _rxSlots[slot];
  out._bind(this, r.buf, r.len, r.flags, r.msg, slot);
  return true;
}

} // namespace nowx
