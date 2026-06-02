#include "NxAckEngine.h"
#include "Log.h"
#include <string.h>

namespace nowx {

NxAckEngine::NxAckEngine(ITransport *t) : _t(t) {}

void NxAckEngine::store(const NxAck &a) {
#ifndef NOWX_HOST_BUILD
    taskENTER_CRITICAL(&_lock);
#endif
    _last  = a;
    _ready = true;
#ifndef NOWX_HOST_BUILD
    taskEXIT_CRITICAL(&_lock);
#endif
}

bool NxAckEngine::wait(uint32_t msg, uint32_t base, uint32_t expected) {
    if (!_t) return false;
    uint32_t st = _t->millis();
    while ((_t->millis() - st) < NX_TIMEOUT_MS) {
        bool  ready = false;
        NxAck snap{};

#ifndef NOWX_HOST_BUILD
        taskENTER_CRITICAL(&_lock);
#endif
        if (_ready) { snap = _last; ready = true; }
#ifndef NOWX_HOST_BUILD
        taskEXIT_CRITICAL(&_lock);
#endif

        if (ready &&
            snap.msg  == msg  &&
            snap.base == base &&
            (snap.ack & expected) == expected)
        {
#ifndef NOWX_HOST_BUILD
            taskENTER_CRITICAL(&_lock);
#endif
            _ready = false;
#ifndef NOWX_HOST_BUILD
            taskEXIT_CRITICAL(&_lock);
#endif
            return true;
        }
        _t->yieldMs(1);
    }
    return false;
}

void NxAckEngine::sendAck(uint32_t msg, uint32_t base, uint32_t ackMap,
                           const uint8_t mac[6]) {
    NxAck a{};
    a.type = NX_IS_ACK;
    a.msg  = msg;
    a.base = base;
    a.ack  = ackMap;
    a.t    = _t ? _t->millis() : 0;
    if (_t) _t->send(mac, reinterpret_cast<uint8_t*>(&a), sizeof(a));
}

} // namespace nowx
