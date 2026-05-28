#pragma once

// ITransport - abstract send/recv used by NxProtocol.
// EspNowTransport wraps ESP-NOW; LoopbackTransport (in tests) wraps memory.

#include <stdint.h>
#include <stddef.h>
#include <functional>

namespace nowx {

  using RecvFn = std::function<void(const uint8_t mac[6],
                                    const uint8_t *data,
                                    size_t len)>;

  class ITransport {
  public:
    virtual ~ITransport() = default;

    // Send `len` bytes to `mac`. Returns true on best-effort accept.
    virtual bool send(const uint8_t mac[6],
                      const uint8_t *data,
                      size_t len) = 0;

    // Register receive callback. Called from arbitrary context.
    virtual void onRecv(RecvFn fn) = 0;

    // Monotonic milliseconds.
    virtual uint32_t millis() = 0;

    // Cooperative yield (sleep ~1ms).
    virtual void yieldMs(uint32_t ms) = 0;
  };
}
