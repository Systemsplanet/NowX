#pragma once

// NxLoopback.h - in-memory two-endpoint transport for unit tests and
// integration testing in user projects.
//
// Supports configurable packet drop rate (dropPercent) to exercise
// the retry/ACK paths without real hardware.
//
// Only compiled when NOWX_HOST_BUILD is defined so it never pulls
// std::deque / std::vector into on-device builds.

#ifdef NOWX_HOST_BUILD

#include "ITransport.h"
#include <vector>
#include <cstring>
#include <cstdlib>

namespace nowx {

  class LoopbackBus;

  // LoopbackEndpoint - one node on the in-memory bus.
  class LoopbackEndpoint : public ITransport {
  public:
    LoopbackEndpoint(LoopbackBus *bus, const uint8_t mac[6]);

    bool     send(const uint8_t mac[6], const uint8_t *data, size_t len) override;
    void     onRecv(RecvFn fn) override { _cb = std::move(fn); }
    uint32_t millis() override;
    void     yieldMs(uint32_t ms) override;

    // Called by LoopbackBus to deliver an inbound packet.
    void deliver(const uint8_t srcMac[6], const uint8_t *data, size_t len) {
      if (_cb) _cb(srcMac, data, len);
    }

    const uint8_t *mac() const { return _mac; }

  private:
    LoopbackBus *_bus;
    uint8_t      _mac[6];
    RecvFn       _cb;
  };

  // LoopbackBus - routes packets between registered endpoints.
  class LoopbackBus {
  public:
    uint32_t now         = 0;  // virtual clock (ms); advance with advance()
    int      dropPercent = 0;  // 0-100; packets dropped at random

    void registerEndpoint(LoopbackEndpoint *e) { _ep.push_back(e); }

    bool route(const uint8_t srcMac[6], const uint8_t dstMac[6],
               const uint8_t *data, size_t len) {
      if (dropPercent > 0 && (rand() % 100) < dropPercent) {
        droppedCount++;
        return true; // silently drop
      }
      for (auto *e : _ep) {
        if (std::memcmp(e->mac(), dstMac, 6) == 0) {
          std::vector<uint8_t> copy(data, data + len);
          e->deliver(srcMac, copy.data(), copy.size());
          return true;
        }
      }
      return false; // unknown destination
    }

    // Advance the virtual clock by `ms` milliseconds.
    void advance(uint32_t ms) { now += ms; }

    int droppedCount = 0;

  private:
    std::vector<LoopbackEndpoint*> _ep;
  };

  // ---- Inline method bodies -------------------------------------------

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

  inline uint32_t LoopbackEndpoint::millis()          { return _bus->now; }
  inline void     LoopbackEndpoint::yieldMs(uint32_t ms) {
    _bus->advance(ms ? ms : 1);
  }

} // namespace nowx

#endif // NOWX_HOST_BUILD
