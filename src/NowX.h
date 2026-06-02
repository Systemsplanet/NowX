#pragma once

// NowX - Arduino/ESP32 facade that wires NxProtocol to ESP-NOW.
// On host builds this header is a no-op; tests use NxProtocol directly.

#include "NxProtocol.h"

#ifndef NOWX_HOST_BUILD
  #include <Arduino.h>
  #include "EspNowTransport.h"

namespace nowx {

  class NowX {
  public:
    // `name` is used only for log output.
    explicit NowX(const char *name);

    // Initialise ESP-NOW.  Call once in setup(). Returns false on error.
    bool begin();

    // Set the single remote peer by MAC string ("AA:BB:CC:DD:EE:FF").
    void setPeer(const char *mac);

    // Set AES-256-GCM encryption key. Pass nullptr/0 to disable.
    void setKey(const uint8_t *key, uint32_t len);

    // Send bytes/String.  Returns true when fully ACKed.
    // May block for up to NX_TIMEOUT_MS * NX_RETRY * numWindows ms.
    bool send(const uint8_t *d, uint32_t len, uint8_t flags = 0);
    bool send(const String &s,               uint8_t flags = 0);

    // Pop one fully-reassembled message into `m`.
    bool receive(Message &m);

    // Access the underlying protocol for advanced use.
    NxProtocol& protocol() { return _p; }

  private:
    char            _name[32];
    EspNowTransport _t;
    NxProtocol      _p;
  };

} // namespace nowx

// Pull the most-used names into the global namespace for sketch convenience.
using nowx::NowX;
using nowx::Message;
using nowx::NX_ENCRYPT;
using nowx::NX_ACK;

#endif // !NOWX_HOST_BUILD
