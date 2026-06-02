#include "NxProtocol.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

namespace nowx {

// =========================================================================
// Message
// =========================================================================

Message::Message(Message &&o) noexcept
    : _owner(o._owner), _slot(o._slot), _ptr(o._ptr),
      _len(o._len), _flags(o._flags), _msg(o._msg)
{
    o._owner = nullptr; o._slot = -1;
    o._ptr   = nullptr; o._len  = 0;
}

Message &Message::operator=(Message &&o) noexcept {
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
        _owner = nullptr; _slot = -1;
        _ptr   = nullptr; _len  = 0;
    }
}

void Message::_bind(NxProtocol *p, uint8_t *ptr, uint32_t len,
                    uint8_t flags, uint32_t msg, int slot) {
    _owner = p; _ptr = ptr; _len = len;
    _flags = flags; _msg = msg; _slot = slot;
}

// =========================================================================
// NxProtocol
// =========================================================================

NxProtocol::NxProtocol(ITransport *t)
    : _t(t),
      _ack(t),
      _queue(NX_RXQ),
      _fragmenter(t, &_ack, &_crypto, _tx.peer, &_tx.peerSet),
      _reassembler(t, &_ack, &_crypto, &_queue, NX_POOL, NX_MSG)
{
    _fragmenter.setCrcFn(&NxProtocol::crc32);
    _reassembler.setCrcFn(&NxProtocol::crc32);

    if (_t) {
        _t->onRecv([this](const uint8_t mac[6], const uint8_t *d, size_t n) {
            this->_onPacket(mac, d, n);
        });
    }
}

NxProtocol::~NxProtocol() = default;

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
    _crypto.setKey(key, len);
}

// =========================================================================
// Send
// =========================================================================

bool NxProtocol::send(const std::string &s, uint8_t flags) {
    return send(reinterpret_cast<const uint8_t*>(s.data()),
                (uint32_t)s.size(), flags);
}

bool NxProtocol::send(const uint8_t *data, uint32_t len, uint8_t flags) {
    if (!_t || !_tx.peerSet)         return false;
    if (len > NX_MAX_PAYLOAD_BYTES)  return false;

    uint32_t msg = ++_tx.msgCtr;
    uint32_t cnt = (len + NX_PAY - 1) / NX_PAY;
    if (cnt == 0)            cnt = 1;
    if (cnt > NX_MAX_BLOCKS) return false;

    return _fragmenter.send(data, len, flags, msg, cnt);
}

// =========================================================================
// Receive
// =========================================================================

void NxProtocol::_onPacket(const uint8_t mac[6],
                            const uint8_t *d, size_t len) {
    if (!d || len == 0) return;
    if (len >= sizeof(NxAck) && d[0] == NX_IS_ACK) {
        NxAck a;
        memcpy(&a, d, sizeof(a));
        _ack.store(a);
        return;
    }
    if (len >= sizeof(NxHdr)) _reassembler.handleData(mac, d, len);
}

bool NxProtocol::receive(Message &out) {
    int slot;
    if (!_queue.dequeue(slot)) return false;
    RxMsg &r = _queue[slot];
    out._bind(this, r.buf, r.len, r.flags, r.msg, slot);
    return true;
}

void NxProtocol::_releaseSlot(int slot) {
    _queue.release(slot);
}

// =========================================================================
// Pure helpers
// =========================================================================

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

bool NxProtocol::encryptFragment(const uint8_t *plain, uint16_t len,
                                  uint8_t *dst, uint16_t &outLen) const {
    return _crypto.encryptFragment(plain, len, dst, outLen);
}

bool NxProtocol::decryptFragment(const uint8_t *enc, uint16_t encLen,
                                  uint8_t *dst, uint16_t &outLen) const {
    return _crypto.decryptFragment(enc, encLen, dst, outLen);
}

} // namespace nowx
