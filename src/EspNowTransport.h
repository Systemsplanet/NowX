#pragma once

#ifndef NOWX_HOST_BUILD

#include "ITransport.h"
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

namespace nowx {

  // EspNowTransport - wraps ESP-NOW for the ESP32 Arduino framework.
  //
  // Only one instance should exist per device; the ESP-NOW receive
  // callback is registered once in begin() and stored in a module-level
  // pointer.  If you need a second logical channel, use a second ESP32.
  class EspNowTransport : public ITransport {
  public:
    EspNowTransport();

    // Initialise WiFi station mode and ESP-NOW.  Call once in setup().
    bool begin();

    // Register a unicast peer. Safe to call multiple times for the same MAC.
    bool addPeer(const uint8_t mac[6]);

    // ITransport
    bool     send(const uint8_t mac[6], const uint8_t *data, size_t len) override;
    void     onRecv(RecvFn fn) override;
    uint32_t millis() override;
    void     yieldMs(uint32_t ms) override;

  private:
    // Module-level singleton pointer — only one transport at a time.
    // A second construction overwrites this and logs a warning.
    static EspNowTransport *_inst;
    RecvFn _cb;

    static void _rxCb(const esp_now_recv_info_t *info,
                      const uint8_t *d, int len);
  };
}
#endif // !NOWX_HOST_BUILD
