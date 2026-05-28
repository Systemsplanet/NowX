#pragma once

#ifndef NOWX_HOST_BUILD

#include "ITransport.h"
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

namespace nowx {

  class EspNowTransport : public ITransport {
  public:
    EspNowTransport();
    bool begin();

    bool addPeer(const uint8_t mac[6]);

    // ITransport
    bool     send(const uint8_t mac[6], const uint8_t *data, size_t len) override;
    void     onRecv(RecvFn fn) override;
    uint32_t millis() override;
    void     yieldMs(uint32_t ms) override;

  private:
    static EspNowTransport *_me;
    RecvFn _cb;

    static void _rxTrampoline(const esp_now_recv_info_t *i,
                              const uint8_t *d, int len);
  };
}
#endif
