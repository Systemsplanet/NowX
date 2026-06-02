#pragma once

#include "NxWire.h"
#include "NxAckEngine.h"
#include "ITransport.h"

namespace nowx {

class NxCrypto;

class NxFragmenter {
public:
    NxFragmenter(ITransport *t, NxAckEngine *ack, NxCrypto *crypto,
                 const uint8_t peer[6], bool *peerSet);

    bool send(const uint8_t *data, uint32_t len,
              uint8_t flags, uint32_t msg, uint32_t cnt);

    // crc32 is a pure function; provided by NxProtocol and injected here
    // to avoid a back-dependency on NxProtocol.h.
    using CrcFn = uint32_t (*)(const uint8_t *, uint32_t);
    void setCrcFn(CrcFn fn) { _crc = fn; }

private:
    ITransport  *_t;
    NxAckEngine *_ack;
    NxCrypto    *_crypto;
    const uint8_t *_peer;
    bool          *_peerSet;
    CrcFn          _crc = nullptr;

    bool _sendRaw(const uint8_t *d, uint32_t len);
};

} // namespace nowx
