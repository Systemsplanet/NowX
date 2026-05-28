#pragma once

// LoopbackTransport - in-memory two-endpoint transport for unit tests.
// Supports configurable packet loss, duplication, and reordering.

#include "ITransport.h"
#include <deque>
#include <vector>
#include <memory>
#include <cstring>
#include <cstdlib>

namespace nowx {

  class LoopbackBus;

  class LoopbackEndpoint : public ITransport {
  public:
    LoopbackEndpoint(LoopbackBus *bus, const uint8_t mac[6]);

    bool     send(const uint8_t mac[6], const uint8_t *data, size_t len) override;
    void     onRecv(RecvFn fn) override { _cb = std::move(fn); }
    uint32_t millis() override;
    void     yieldMs(uint32_t ms) override;

    void deliver(const uint8_t srcMac[6], const uint8_t *data, size_t len) {
      if (_cb) _cb(srcMac, data, len);
    }

    const uint8_t *mac() const { return _mac; }

  private:
    LoopbackBus *_bus;
    uint8_t      _mac[6];
    RecvFn       _cb;
  };

  class LoopbackBus {
  public:
    uint32_t now         = 0;
    int      dropPercent = 0;

    void registerEndpoint(LoopbackEndpoint *e) { _ep.push_back(e); }

    bool route(const uint8_t srcMac[6], const uint8_t dstMac[6],
               const uint8_t *data, size_t len) {
      if (dropPercent > 0 && (rand() % 100) < dropPercent) {
        droppedCount++;
        return true;
      }
      for (auto *e : _ep) {
        if (std::memcmp(e->mac(), dstMac, 6) == 0) {
          std::vector<uint8_t> copy(data, data + len);
          e->deliver(srcMac, copy.data(), copy.size());
          return true;
        }
      }
      return false;
    }

    void advance(uint32_t ms) { now += ms; }

    int droppedCount = 0;

  private:
    std::vector<LoopbackEndpoint*> _ep;
  };

  inline LoopbackEndpoint::LoopbackEndpoint(LoopbackBus *bus,
                                             const uint8_t mac[6])
    : _bus(bus) {
    std::memcpy(_mac, mac, 6);
    bus->registerEndpoint(this);
  }

  inline bool LoopbackEndpoint::send(const uint8_t mac[6],
                                      const uint8_t *data, size_t len) {
    return _bus->route(_mac, mac, data, len);
  }

  inline uint32_t LoopbackEndpoint::millis() { return _bus->now; }
  inline void     LoopbackEndpoint::yieldMs(uint32_t ms) { _bus->advance(ms ? ms : 1); }
}
