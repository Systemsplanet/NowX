#include "NxReassembler.h"
#include "NxCrypto.h"
#include <string.h>
#include <stdlib.h>

namespace nowx {

NxReassembler::NxReassembler(ITransport *t, NxAckEngine *ack,
                              NxCrypto *crypto, NxQueue *queue,
                              int poolSize, int asmSlots)
    : _t(t), _ack(ack), _crypto(crypto), _queue(queue),
      _poolSize(poolSize), _asmSlots(asmSlots)
{
    _pool = new Frag[poolSize]();
    _asm  = new RxAsm[asmSlots]();
}

NxReassembler::~NxReassembler() {
    if (_pool) {
        for (int i = 0; i < _poolSize; i++) _freeFragData(&_pool[i]);
        delete[] _pool;
        _pool = nullptr;
    }
    if (_asm) {
        for (int i = 0; i < _asmSlots; i++) {
            if (_asm[i].frag)   { free(_asm[i].frag);   _asm[i].frag   = nullptr; }
            if (_asm[i].outBuf) { free(_asm[i].outBuf); _asm[i].outBuf = nullptr; }
        }
        delete[] _asm;
        _asm = nullptr;
    }
}

void NxReassembler::handleData(const uint8_t mac[6],
                                const uint8_t *d, size_t len)
{
    NxHdr h;
    memcpy(&h, d, sizeof(h));

    if (h.magic != NX_MAGIC)                   return;
    if (h.cnt   == 0)                          return;
    if (h.cnt   >  (uint32_t)NX_MAX_BLOCKS)   return;
    if (h.idx   >= h.cnt)                      return;
    if (h.pay   >  NX_ENC_PAY)                return;
    if (len < sizeof(NxHdr) + h.pay)           return;

    const uint8_t *wirePay = d + sizeof(NxHdr);
    if (_crc ? _crc(wirePay, h.pay) : 0 != h.crc) return;

    if (isDup(mac, h.msg)) {
        uint32_t base = (h.idx / NX_WIN) * NX_WIN;
        uint32_t end  = (base + NX_WIN < h.cnt) ? base + NX_WIN : h.cnt;
        uint32_t map  = 0;
        for (uint32_t b = base; b < end; b++) map |= (1UL << (b - base));
        _ack->sendAck(h.msg, base, map, mac);
        return;
    }

    RxAsm *a = _asmGet(h.msg, mac);
    if (!a) return;

    if (a->cnt == 0) {
        if (h.cnt > (uint32_t)NX_MAX_BLOCKS) return;

        uint32_t maxBytes = (uint32_t)h.cnt * (uint32_t)NX_PAY;
        if (maxBytes > NX_MAX_PAYLOAD_BYTES) return;

        a->outBuf = static_cast<uint8_t*>(malloc(maxBytes));
        if (!a->outBuf) return;

        a->outCap    = maxBytes;
        a->outLen    = 0;
        a->flushedTo = 0;
        a->cnt       = h.cnt;

        if (!_ensureAsmFrags(a, NX_WIN)) {
            free(a->outBuf); a->outBuf = nullptr;
            a->cnt = 0;
            return;
        }
    }

    a->lastMs = _t ? _t->millis() : 0;

    uint32_t base = (h.idx / NX_WIN) * NX_WIN;
    uint32_t slot = h.idx - base;

    if (h.idx < a->flushedTo) {
        uint32_t end = (base + NX_WIN < h.cnt) ? base + NX_WIN : h.cnt;
        uint32_t map = 0;
        for (uint32_t b = base; b < end; b++) map |= (1UL << (b - base));
        _ack->sendAck(h.msg, base, map, mac);
        return;
    }

    if (!a->frag[slot]) {
        Frag *f = _allocFrag(h.pay);
        if (!f) return;

        if (h.flags & NX_ENCRYPT) {
            uint16_t ptLen = 0;
            if (!_crypto->decryptFragment(wirePay, h.pay, f->data, ptLen)) {
                _freeFrag(f);
                return;
            }
            f->len = ptLen;
        } else {
            memcpy(f->data, wirePay, h.pay);
            f->len = h.pay;
        }

        a->frag[slot] = f;
        a->got++;
    }

    uint32_t end = (base + NX_WIN < h.cnt) ? base + NX_WIN : h.cnt;
    uint32_t map = 0;
    for (uint32_t b = base; b < end; b++) {
        if (a->frag[b - base]) map |= (1UL << (b - base));
    }
    _ack->sendAck(h.msg, base, map, mac);

    uint32_t winFrags = end - base;
    uint32_t winGot   = 0;
    for (uint32_t s = 0; s < winFrags; s++) {
        if (a->frag[s]) winGot++;
    }

    if (winGot == winFrags) {
        for (uint32_t s = 0; s < winFrags; s++) {
            Frag *f = a->frag[s];
            uint32_t writeOff = (base + s) * NX_PAY;
            memcpy(a->outBuf + writeOff, f->data, f->len);
            if (base + s == a->cnt - 1) {
                a->outLen = writeOff + f->len;
            }
            _freeFrag(f);
            a->frag[s] = nullptr;
        }
        a->flushedTo = end;
    }

    if (a->flushedTo != a->cnt) return;

    if (a->outLen == 0 && a->cnt > 0) {
        a->outLen = a->outCap;
    }

    int rxSlot = _queue->alloc();
    if (rxSlot < 0) {
        free(a->outBuf); a->outBuf = nullptr;
        a->used = false; a->msg = 0; a->cnt = 0; a->got = 0;
        a->outLen = 0; a->outCap = 0; a->flushedTo = 0;
        if (a->frag) memset(a->frag, 0, a->fragCap * sizeof(Frag*));
        return;
    }

    RxMsg &r = (*_queue)[rxSlot];
    uint8_t *exact = static_cast<uint8_t*>(realloc(a->outBuf, a->outLen ? a->outLen : 1));
    if (exact) {
        r.buf = exact;
    } else {
        r.buf = a->outBuf;
    }
    a->outBuf = nullptr;

    r.len   = a->outLen;
    r.flags = h.flags;
    r.msg   = h.msg;

    if (!_queue->enqueue(rxSlot)) {
        _queue->release(rxSlot);
    } else {
        markSeen(mac, h.msg);
    }

    a->used = false; a->msg = 0; a->cnt = 0; a->got = 0;
    a->outLen = 0; a->outCap = 0; a->flushedTo = 0;
    if (a->frag) memset(a->frag, 0, a->fragCap * sizeof(Frag*));
}

bool NxReassembler::isDup(const uint8_t mac[6], uint32_t msg) const {
    for (auto &e : _dedup) {
        if (e.msg == msg && memcmp(e.mac, mac, 6) == 0) return true;
    }
    return false;
}

void NxReassembler::markSeen(const uint8_t mac[6], uint32_t msg) {
    auto &e = _dedup[_dedupHead];
    memcpy(e.mac, mac, 6);
    e.msg      = msg;
    _dedupHead = (uint8_t)((_dedupHead + 1) % NX_DUP);
}

void NxReassembler::_freeFragData(Frag *f) {
    if (!f) return;
    if (f->data) { free(f->data); f->data = nullptr; }
    f->dataCap = 0;
    f->len     = 0;
    f->used    = false;
}

NxReassembler::Frag *NxReassembler::_allocFrag(uint16_t neededCap) {
    for (int i = 0; i < _poolSize; i++) {
        Frag &f = _pool[i];
        if (f.used) continue;
        if (f.dataCap < neededCap) {
            uint8_t *nb = static_cast<uint8_t*>(realloc(f.data, neededCap));
            if (!nb) return nullptr;
            f.data    = nb;
            f.dataCap = neededCap;
        }
        f.used = true;
        f.len  = 0;
        return &f;
    }
    return nullptr;
}

void NxReassembler::_freeFrag(Frag *f) {
    if (f) { f->used = false; f->len = 0; }
}

bool NxReassembler::_ensureAsmFrags(RxAsm *a, uint32_t cnt) {
    if (!a) return false;
    if (a->fragCap >= cnt) return true;
    if (cnt > NX_MAX_BLOCKS) return false;

    Frag **nb = static_cast<Frag**>(realloc(a->frag, cnt * sizeof(Frag*)));
    if (!nb) return false;

    memset(nb + a->fragCap, 0, (cnt - a->fragCap) * sizeof(Frag*));
    a->frag    = nb;
    a->fragCap = cnt;
    return true;
}

NxReassembler::RxAsm *NxReassembler::_asmGet(uint32_t msg,
                                               const uint8_t mac[6])
{
    _asmGc();
    for (int i = 0; i < _asmSlots; i++) {
        RxAsm &a = _asm[i];
        if (a.used && a.msg == msg && memcmp(a.srcMac, mac, 6) == 0) return &a;
    }
    for (int i = 0; i < _asmSlots; i++) {
        RxAsm &a = _asm[i];
        if (!a.used) {
            a.used      = true;
            a.msg       = msg;
            memcpy(a.srcMac, mac, 6);
            a.cnt       = 0;
            a.got       = 0;
            a.flushedTo = 0;
            a.outLen    = 0;
            a.outCap    = 0;
            a.outBuf    = nullptr;
            a.lastMs    = _t ? _t->millis() : 0;
            if (a.frag) memset(a.frag, 0, a.fragCap * sizeof(Frag*));
            return &a;
        }
    }
    return nullptr;
}

void NxReassembler::_asmGc() {
    if (!_t) return;
    uint32_t now = _t->millis();
    for (int i = 0; i < _asmSlots; i++) {
        RxAsm &a = _asm[i];
        if (!a.used || (now - a.lastMs) <= NX_TTL) continue;

        for (uint32_t j = 0; j < a.fragCap; j++) {
            if (a.frag[j]) { _freeFrag(a.frag[j]); a.frag[j] = nullptr; }
        }
        if (a.outBuf) { free(a.outBuf); a.outBuf = nullptr; }

        a.used = false; a.msg = 0; a.cnt = 0; a.got = 0;
        a.flushedTo = 0; a.outLen = 0; a.outCap = 0;
        if (a.frag) memset(a.frag, 0, a.fragCap * sizeof(Frag*));
    }
}

} // namespace nowx
