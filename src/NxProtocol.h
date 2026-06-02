#pragma once

// NxProtocol - top-level protocol engine.
//
// Composition:
//   NxProtocol
//    ├─ NxCrypto       — AES-256-GCM key management and fragment crypto
//    ├─ NxAckEngine    — ACK store, wait, and send
//    ├─ NxFragmenter   — TX windowed fragmentation with retries
//    ├─ NxReassembler  — RX sliding-window reassembly, dedup, GC
//    └─ NxQueue        — Circular RX message queue
//
// Sub-layers include NxWire.h for shared wire types and tunables.
// This header is the only include needed by application code or NowX.h.

#include "NxWire.h"
#include "NxCrypto.h"
#include "NxAckEngine.h"
#include "NxQueue.h"
#include "NxFragmenter.h"
#include "NxReassembler.h"
#include "ITransport.h"

#include <string>

namespace nowx {

class NxProtocol;

// ---- Message handle ------------------------------------------------------
class Message {
public:
    Message() = default;
    ~Message() { release(); }

    Message(const Message &)            = delete;
    Message &operator=(const Message &) = delete;
    Message(Message &&o) noexcept;
    Message &operator=(Message &&o) noexcept;

    uint8_t  *data()      const { return _ptr; }
    uint32_t  len()       const { return _len; }
    uint32_t  id()        const { return _msg; }
    bool      encrypted() const { return (_flags & NX_ENCRYPT) != 0; }

    std::string str() const;
    void release();

    void _bind(NxProtocol *p, uint8_t *ptr, uint32_t len,
               uint8_t flags, uint32_t msg, int slot);

private:
    NxProtocol *_owner = nullptr;
    int         _slot  = -1;
    uint8_t    *_ptr   = nullptr;
    uint32_t    _len   = 0;
    uint8_t     _flags = 0;
    uint32_t    _msg   = 0;
};

// ---- Protocol engine -----------------------------------------------------
class NxProtocol {
public:
    explicit NxProtocol(ITransport *t);
    ~NxProtocol();

    NxProtocol(const NxProtocol &)            = delete;
    NxProtocol &operator=(const NxProtocol &) = delete;

    void setPeer(const uint8_t mac[6]);
    bool setPeerStr(const char *s);

    void setKey(const uint8_t *key, uint32_t len);

    bool send(const uint8_t *data, uint32_t len, uint8_t flags = 0);
    bool send(const std::string &s,              uint8_t flags = 0);

    bool receive(Message &out);

    // Pure helpers — also exposed for testing.
    static uint32_t crc32(const uint8_t *d, uint32_t len);
    static bool     parseMac(const char *s, uint8_t mac[6]);

    bool encryptFragment(const uint8_t *plain, uint16_t len,
                         uint8_t *dst, uint16_t &outLen) const;
    bool decryptFragment(const uint8_t *enc, uint16_t encLen,
                         uint8_t *dst, uint16_t &outLen) const;

    void _releaseSlot(int slot);
    void _onPacket(const uint8_t mac[6], const uint8_t *d, size_t len);

    bool hasEncryption() const { return _crypto.hasKey(); }

private:
    struct TxState {
        uint8_t  peer[6]{};
        bool     peerSet = false;
        uint32_t msgCtr  = 0;
    } _tx;

    ITransport    *_t;
    NxCrypto       _crypto;
    NxAckEngine    _ack;
    NxQueue        _queue;
    NxFragmenter   _fragmenter;
    NxReassembler  _reassembler;
};

} // namespace nowx
