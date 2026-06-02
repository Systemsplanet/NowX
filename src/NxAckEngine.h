#pragma once

#include "NxWire.h"
#include "ITransport.h"

#ifndef NOWX_HOST_BUILD
  #include <freertos/FreeRTOS.h>
  #include <freertos/task.h>
#endif

namespace nowx {

class NxAckEngine {
public:
    explicit NxAckEngine(ITransport *t);

    void store(const NxAck &a);
    bool wait(uint32_t msg, uint32_t base, uint32_t expected);
    void sendAck(uint32_t msg, uint32_t base, uint32_t ackMap,
                 const uint8_t mac[6]);

private:
    ITransport *_t;

#ifdef NOWX_HOST_BUILD
    bool   _ready = false;
    NxAck  _last{};
#else
    volatile bool _ready = false;
    NxAck         _last{};
    portMUX_TYPE  _lock  = portMUX_INITIALIZER_UNLOCKED;
#endif
};

} // namespace nowx
