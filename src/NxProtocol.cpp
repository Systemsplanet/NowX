#include "NxProtocol.h"
#include "Log.h"
#include <string.h>
#include <stdio.h>

namespace nowx {

  // ====================================================================
  // Message
  // ====================================================================
  std::string Message::str() const {
    if (!_ptr || _len == 0) return std::string();
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

  // ====================================================================
  // NxProtocol - construction & config
  // ====================================================================
  NxProtocol::NxProtocol(ITransport *t) : _t(t) {
    if (_t) {
      _t->onRecv([this](const uint8_t mac[6],
                        const uint8_t *d, size_t n) {
        this->_onPacket(mac, d, n);
      });
    }
  }

  void NxProtocol::setPeer(const uint8_t mac[6]) {
    memcpy(_peer, mac, 6);
    _peerSet = true;
  }

  bool NxProtocol::setPeerStr(const char *s) {
    uint8_t m[6];
    if (!parseMac(s, m)) return false;
    setPeer(m);
    return true;
  }

  void NxProtocol::setKey(const uint8_t *key, uint32_t len) {
    memset(_key, 0, sizeof(_key));
    if (!key || len == 0) { _enc = false; return; }
    uint32_t n = len < 32 ? len : 32;
    memcpy(_key, key, n);
    _enc = true;
  }

  // ====================================================================
  // Pure helpers
  // ====================================================================
  uint32_t NxProtocol::crc32(const uint8_t *d, uint32_t len) {
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
    if (!_enc) return;
    for (uint32_t i = 0; i < len; i++) d[i] ^= _key[i & 31];
  }

  // ====================================================================
  // Pool / slot management
  // ====================================================================
  NxProtocol::Seg *NxProtocol::_alloc() {
    for (auto &s : _pool) {
      if (!s.used) { s.used = true; return &s; }
    }
    return nullptr;
  }
  void NxProtocol::_free(Seg *s) { if (s) s->used = false; }

  NxProtocol::Asm *NxProtocol::_asmGet(uint32_t msg, const uint8_t mac[6]) {
    _asmGc();
    for (auto &a : _asm) {
      if (a.used && a.msg == msg && memcmp(a.srcMac, mac, 6) == 0) return &a;
    }
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
    return nullptr;
  }

  void NxProtocol::_asmGc() {
    if (!_t) return;
    uint32_t now = _t->millis();
    for (auto &a : _asm) {
      if (a.used && (now - a.lastMs) > NX_ASM_TTL_MS) {
        for (uint16_t i = 0; i < a.total; i++) _free(a.seg[i]);
        memset(&a, 0, sizeof(a));
      }
    }
  }

  int NxProtocol::_rxAlloc() {
    for (int i = 0; i < NX_RX_QUEUE; i++) {
      if (!_rxSlots[i].used) { _rxSlots[i].used = true; return i; }
    }
    return -1;
  }

  void NxProtocol::_releaseSlot(int slot) {
    if (slot < 0 || slot >= NX_RX_QUEUE) return;
    _rxSlots[slot].used = false;
    _rxSlots[slot].len  = 0;
  }

  // ====================================================================
  // Dedup
  // ====================================================================
  bool NxProtocol::_isDuplicate(const uint8_t mac[6], uint32_t msg) {
    for (auto &e : _dedup) {
      if (e.msg == msg && memcmp(e.mac, mac, 6) == 0) return true;
    }
    return false;
  }
  void NxProtocol::_markSeen(const uint8_t mac[6], uint32_t msg) {
    auto &e = _dedup[_dedupHead];
    memcpy(e.mac, mac, 6);
    e.msg = msg;
    _dedupHead = (uint8_t)((_dedupHead + 1) % NX_DEDUP);
  }

  // ====================================================================
  // Send path
  // ====================================================================
  bool NxProtocol::_sendRaw(const uint8_t *d, uint32_t len) {
    if (!_t || !_peerSet) return false;
    return _t->send(_peer, d, len);
  }

  bool NxProtocol::send(const std::string &s, uint8_t flags) {
    return send(reinterpret_cast<const uint8_t*>(s.data()),
                (uint32_t)s.size(), flags);
  }

  bool NxProtocol::send(const uint8_t *data, uint32_t len, uint8_t flags) {
    if (!_t || !_peerSet) return false;
    if (len > NX_MAX_BYTES) return false;

    uint32_t msg   = ++_msgCtr;
    uint16_t total = (uint16_t)((len + NX_PAY - 1) / NX_PAY);
    if (total == 0) total = 1;
    if (total > NX_MAX_BLOCKS) return false;

    uint8_t pkt[sizeof(NxHdr) + NX_PAY];

    for (uint16_t base = 0; base < total; base += NX_WIN) {
      uint16_t end = (uint16_t)((base + NX_WIN < total)
                                ? (base + NX_WIN) : total);

      uint32_t expected = 0;
      for (uint16_t b = base; b < end; b++) expected |= (1UL << (b - base));

      bool ok = false;
      for (uint8_t r = 0; r < NX_RETRY && !ok; r++) {
        for (uint16_t b = base; b < end; b++) {
          uint32_t off = (uint32_t)b * NX_PAY;
          uint16_t sz  = (uint16_t)((len - off > NX_PAY)
                                     ? NX_PAY : (len - off));

          uint8_t *wirePay = pkt + sizeof(NxHdr);
          if (sz) memcpy(wirePay, data + off, sz);
          if (flags & NX_OBFUSCATE) xorInPlace(wirePay, sz);

          NxHdr h{};
          h.magic = NX_MAGIC;
          h.msg   = msg;
          h.blk   = b;
          h.total = total;
          h.len   = sz;
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

  bool NxProtocol::_waitAck(uint32_t msg, uint32_t base,
                             uint32_t expectedMap) {
    if (!_t) return false;
    uint32_t st = _t->millis();
    while ((_t->millis() - st) < NX_TIMEOUT_MS) {
      if (_ackPending &&
          _lastAck.msg  == msg  &&
          _lastAck.base == base &&
          (_lastAck.map & expectedMap) == expectedMap) {
        _ackPending = false;
        return true;
      }
      _t->yieldMs(1);
    }
    return false;
  }

  void NxProtocol::_sendAck(uint32_t msg, uint32_t base, uint32_t map,
                             const uint8_t mac[6]) {
    NxAck a{};
    a.msg  = msg;
    a.base = base;
    a.map  = map;
    a.ts   = _t ? _t->millis() : 0;
    if (_t) _t->send(mac, reinterpret_cast<uint8_t*>(&a), sizeof(a));
  }

  // ====================================================================
  // Receive path
  // ====================================================================
  void NxProtocol::_onPacket(const uint8_t mac[6],
                              const uint8_t *d, size_t len) {
    if (!d) return;
    if (len == sizeof(NxAck)) { _handleAck(d, len); return; }
    if (len >= sizeof(NxHdr)) _handleData(mac, d, len);
  }

  void NxProtocol::_handleAck(const uint8_t *d, size_t /*len*/) {
    NxAck a;
    memcpy(&a, d, sizeof(a));
    _lastAck    = a;
    _ackPending = true;
  }

  void NxProtocol::_handleData(const uint8_t mac[6],
                                const uint8_t *d, size_t len) {
    NxHdr h;
    memcpy(&h, d, sizeof(h));
    if (h.magic   != NX_MAGIC)    return;
    if (h.total   == 0)            return;
    if (h.total   >  NX_MAX_BLOCKS) return;
    if (h.blk     >= h.total)      return;
    if (h.len     >  NX_PAY)       return;
    if (len < sizeof(NxHdr) + h.len) return;

    const uint8_t *wirePay = d + sizeof(NxHdr);

    if (crc32(wirePay, h.len) != h.crc) return;

    if (_isDuplicate(mac, h.msg)) {
      uint32_t base = (h.blk / NX_WIN) * NX_WIN;
      uint16_t end  = (uint16_t)((base + NX_WIN < h.total)
                                   ? (base + NX_WIN) : h.total);
      uint32_t map  = 0;
      for (uint16_t b = base; b < end; b++) map |= (1UL << (b - base));
      _sendAck(h.msg, base, map, mac);
      return;
    }

    Asm *a = _asmGet(h.msg, mac);
    if (!a) return;
    a->total  = h.total;
    a->lastMs = _t ? _t->millis() : 0;

    if (!a->seg[h.blk]) {
      Seg *s = _alloc();
      if (!s) return;
      memcpy(s->data, wirePay, h.len);
      if (h.flags & NX_OBFUSCATE) xorInPlace(s->data, h.len);
      s->len = h.len;
      a->seg[h.blk] = s;
      a->got++;
      a->winMap[h.blk / 32] |= (1UL << (h.blk & 31));
    }

    uint32_t base = (h.blk / NX_WIN) * NX_WIN;
    uint16_t end  = (uint16_t)((base + NX_WIN < h.total)
                                 ? (base + NX_WIN) : h.total);
    uint32_t map  = 0;
    for (uint16_t b = base; b < end; b++) {
      if (a->seg[b]) map |= (1UL << (b - base));
    }
    _sendAck(h.msg, base, map, mac);

    if (a->got == a->total) {
      int slot = _rxAlloc();
      if (slot < 0) return;
      RxSlot &r = _rxSlots[slot];
      uint32_t pos = 0;
      for (uint16_t i = 0; i < a->total; i++) {
        Seg *x = a->seg[i];
        if (!x) { _releaseSlot(slot); return; }
        if (pos + x->len > NX_MAX_BYTES) { _releaseSlot(slot); return; }
        memcpy(r.buf + pos, x->data, x->len);
        pos += x->len;
        _free(x);
      }
      r.len   = pos;
      r.flags = h.flags;
      r.msg   = h.msg;

      uint8_t head = _rh;
      uint8_t next = (uint8_t)((head + 1) % NX_RX_QUEUE);
      if (next == _rt) {
        _releaseSlot(slot);
      } else {
        _rxQueue[head] = (uint8_t)slot;
        _rh = next;
        _markSeen(mac, h.msg);
      }
      memset(a, 0, sizeof(*a));
    }
  }

  bool NxProtocol::receive(Message &out) {
    if (_rt == _rh) return false;
    uint8_t slot = _rxQueue[_rt];
    _rt = (uint8_t)((_rt + 1) % NX_RX_QUEUE);
    RxSlot &r = _rxSlots[slot];
    out._bind(this, r.buf, r.len, r.flags, r.msg, slot);
    return true;
  }
}
