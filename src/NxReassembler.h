#pragma once

#include "NxWire.h"
#include "NxAckEngine.h"
#include "NxQueue.h"
#include "ITransport.h"

namespace nowx {

class NxCrypto;

class NxReassembler {
public:
    struct Frag {
        bool     used    = false;
        uint16_t len     = 0;
        uint8_t *data    = nullptr;
        uint16_t dataCap = 0;
    };

    struct RxAsm {
        bool     used      = false;
        uint32_t msg       = 0;
        uint8_t  srcMac[6]{};
        uint32_t cnt       = 0;
        uint32_t got       = 0;
        uint32_t flushedTo = 0;
        uint32_t lastMs    = 0;
        Frag   **frag      = nullptr;
        uint32_t fragCap   = 0;
        uint8_t *outBuf    = nullptr;
        uint32_t outCap    = 0;
        uint32_t outLen    = 0;
    };

    using CrcFn = uint32_t (*)(const uint8_t *, uint32_t);

    NxReassembler(ITransport *t, NxAckEngine *ack,
                  NxCrypto *crypto, NxQueue *queue,
                  int poolSize, int asmSlots);
    ~NxReassembler();

    NxReassembler(const NxReassembler &)            = delete;
    NxReassembler &operator=(const NxReassembler &) = delete;

    void setCrcFn(CrcFn fn) { _crc = fn; }

    void handleData(const uint8_t mac[6], const uint8_t *d, size_t len);

    bool isDup(const uint8_t mac[6], uint32_t msg) const;
    void markSeen(const uint8_t mac[6], uint32_t msg);

private:
    ITransport  *_t;
    NxAckEngine *_ack;
    NxCrypto    *_crypto;
    NxQueue     *_queue;
    CrcFn        _crc = nullptr;

    int    _poolSize;
    int    _asmSlots;
    Frag  *_pool;
    RxAsm *_asm;

    struct DedupEntry {
        uint8_t  mac[6]{};
        uint32_t msg = 0;
    };
    DedupEntry _dedup[NX_DUP]{};
    uint8_t    _dedupHead = 0;

    void   _freeFragData(Frag *f);
    Frag  *_allocFrag(uint16_t neededCap);
    void   _freeFrag(Frag *f);
    bool   _ensureAsmFrags(RxAsm *a, uint32_t cnt);
    RxAsm *_asmGet(uint32_t msg, const uint8_t mac[6]);
    void   _asmGc();
};

} // namespace nowx
