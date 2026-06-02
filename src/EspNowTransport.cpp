#include "EspNowTransport.h"

#ifndef NOWX_HOST_BUILD
#include "Log.h"
#include <string.h>

namespace nowx {

  EspNowTransport *EspNowTransport::_inst = nullptr;

  EspNowTransport::EspNowTransport() {
    if (_inst) {
      // Multiple instances share the same ESP-NOW callback slot; warn loudly.
      logln("nowx: WARNING — second EspNowTransport created; "
            "only the latest instance receives packets");
    }
    _inst = this;
  }

  bool EspNowTransport::begin() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    WiFi.setSleep(false);
    esp_wifi_set_ps(WIFI_PS_NONE);

    if (esp_now_init() != ESP_OK) return false;
    esp_now_register_recv_cb(&EspNowTransport::_rxCb);
    return true;
  }

  bool EspNowTransport::addPeer(const uint8_t mac[6]) {
    if (esp_now_is_peer_exist(mac)) return true;
    esp_now_peer_info_t p{};
    memcpy(p.peer_addr, mac, 6);
    p.encrypt = false;
    return esp_now_add_peer(&p) == ESP_OK;
  }

  bool EspNowTransport::send(const uint8_t mac[6],
                              const uint8_t *data, size_t len) {
    return esp_now_send(mac, data, len) == ESP_OK;
  }

  void EspNowTransport::onRecv(RecvFn fn) { _cb = std::move(fn); }

  uint32_t EspNowTransport::millis() { return ::millis(); }

  void EspNowTransport::yieldMs(uint32_t ms) { delay(ms ? ms : 1); }

  void EspNowTransport::_rxCb(const esp_now_recv_info_t *info,
                               const uint8_t *d, int len) {
    if (!_inst || !_inst->_cb || !info || len <= 0) return;
    _inst->_cb(info->src_addr, d, (size_t)len);
  }

} // namespace nowx
#endif // !NOWX_HOST_BUILD
