#pragma once

// NowX - Arduino/ESP32 facade that wires NxProtocol to ESP-NOW.
// On host builds, this header is a no-op; tests use NxProtocol directly.

#include "NxProtocol.h"

#ifndef NOWX_HOST_BUILD
  #include <Arduino.h>
  #include "EspNowTransport.h"

namespace nowx {

  class NowX {
  public:
    explicit NowX(const char *name);

    bool begin();

    void setPeer(const char *mac);
    void setKey(const uint8_t *key, uint32_t len);

    bool send(const uint8_t *d, uint32_t len, uint8_t flags = 0);
    bool send(const String &s, uint8_t flags = 0);

    bool receive(Message &m);

    NxProtocol& protocol() { return _p; }

  private:
    char           _name[32];
    EspNowTransport _t;
    NxProtocol      _p;
  };
}

using nowx::NowX;
using nowx::Message;
using nowx::NX_OBFUSCATE;
using nowx::NX_ACK;

#endif // !NOWX_HOST_BUILD
