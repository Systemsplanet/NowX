#include "NxFragmenter.h"
#include "NxCrypto.h"
#include "Log.h"
#include <string.h>
#include <stdlib.h>

namespace nowx {

NxFragmenter::NxFragmenter(ITransport *t, NxAckEngine *ack,
                            NxCrypto *crypto,
                            const uint8_t peer[6], bool *peerSet)
    : _t(t), _ack(ack), _crypto(crypto), _peer(peer), _peerSet(peerSet) {}

bool NxFragmenter::_sendRaw(const uint8_t *d, uint32_t len) {
    if (!_t || !*_peerSet) return false;
    return _t->send(_peer, d, len);
}

bool NxFragmenter::send(const uint8_t *data, uint32_t len,
                        uint8_t flags, uint32_t msg, uint32_t cnt)
{
    uint8_t pkt[sizeof(NxHdr) + NX_ENC_PAY];

    for (uint32_t base = 0; base < cnt; base += NX_WIN) {
        uint32_t end = (base + NX_WIN < cnt) ? base + NX_WIN : cnt;

        uint32_t expected = 0;
        for (uint32_t b = base; b < end; b++) expected |= (1UL << (b - base));

        bool ok = false;
        for (uint8_t r = 0; r < NX_RETRY && !ok; r++) {

            for (uint32_t b = base; b < end; b++) {
                uint32_t off     = b * (uint32_t)NX_PAY;
                uint16_t sz      = (uint16_t)((len - off > NX_PAY) ? NX_PAY : len - off);
                uint8_t *wirePay = pkt + sizeof(NxHdr);
                uint16_t wireSz  = sz;

                if (flags & NX_ENCRYPT) {
                    if (!_crypto->encryptFragment(data + off, sz, wirePay, wireSz))
                        return false;
                } else {
                    if (sz) memcpy(wirePay, data + off, sz);
                }

                NxHdr h{};
                h.magic = NX_MAGIC;
                h.msg   = msg;
                h.idx   = b;
                h.cnt   = cnt;
                h.pay   = wireSz;
                h.flags = flags;
                h.crc   = _crc ? _crc(wirePay, wireSz) : 0;
                memcpy(pkt, &h, sizeof(h));

                if (!_sendRaw(pkt, (uint32_t)(sizeof(h) + wireSz))) return false;
            }

            if (_ack->wait(msg, base, expected)) {
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

} // namespace nowx
