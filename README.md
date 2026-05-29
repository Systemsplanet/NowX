# NowX

Reliable, fragmented messaging over **ESP-NOW** for ESP32.

NowX wraps the raw ~250-byte ESP-NOW frame with:

- **Fragmentation & reassembly** of arbitrary-size messages (up to `NX_MAX_BLOCKS * NX_PAY`, default ≈ 60 KB).
- **Windowed ACKs with retries** for delivery reliability.
- **CRC32** integrity check per fragment.
- **Optional XOR obfuscation** of payloads (see ⚠️ security note below).
- A **host-testable protocol core** (`NxProtocol`) decoupled from Arduino/ESP-IDF, so the bulk of the logic runs under Unity on your laptop.

---

## ⚠️ Security

`NX_OBFUSCATE` applies XOR with a repeating key. **This is not cryptography.** It discourages casual packet inspection only. For real confidentiality, configure ESP-NOW's built-in LMK/PMK or layer AES-GCM on top.

---

## Install

### PlatformIO

```ini
[env:esp32dev]
platform  = espressif32
board     = esp32dev
framework = arduino
lib_deps  = file:///path/to/NowX   ; or a git URL
```

### Arduino IDE

Copy this folder into `~/Arduino/libraries/NowX/`.

---

## Quick start

### Receiver

```cpp
#include <NowX.h>
#include <WiFi.h>

NowX now("rx");

void setup() {
  Serial.begin(115200);
  now.begin();
  Serial.print("MAC: ");
  Serial.println(WiFi.macAddress());
}

void loop() {
  Message m;
  if (now.receive(m)) {
    Serial.printf("got %u bytes: %s\n", m.len(), m.str().c_str());
    // m.release() is called automatically when m goes out of scope.
  }
}
```

### Sender

```cpp
#include <NowX.h>

NowX now("tx");

void setup() {
  Serial.begin(115200);
  now.begin();
  now.setPeer("AA:BB:CC:DD:EE:FF");
}

void loop() {
  bool ok = now.send(String("hello"));
  Serial.printf("send -> %s\n", ok ? "ok" : "FAIL");
  delay(1000);
}
```

See `examples/` for `Sender`, `Receiver`, and `Encrypted` sketches.

---

## Wire format

All multi-byte fields are little-endian (ESP32 native).

```
NxHdr (17 bytes, packed):
  uint32_t magic  = 0x4E585031 ('NXP1')
  uint32_t msg    monotonically increasing message id
  uint16_t idx    fragment index within this message
  uint16_t cnt    total fragment count
  uint16_t pay    payload bytes in this frame (<= NX_PAY)
  uint8_t  flags  NX_OBFUSCATE | ...
  uint32_t crc    CRC32 of on-wire payload bytes

Followed by `pay` payload bytes (possibly obfuscated).

NxAck (17 bytes, packed):
  uint8_t  type   NX_IS_ACK (0x80) — distinguishes ACK frames from data
  uint32_t msg    message being acked
  uint32_t base   window base fragment index this ack covers
  uint32_t ack    bitmap of received fragments within this window
  uint32_t t      sender timestamp (informational, ms)
```

CRC is computed on the **on-wire** bytes, so the receiver validates CRC before any deobfuscation.

ACK frames are distinguished from data frames by the `type` field at byte 0, not by packet length alone.

---

## Sliding-window ACK

```
sender                          receiver
  | --- frag 0..NX_WIN-1 --->  |
  |                             | store, ack(bitmap, base=0)
  | <-------- NxAck base=0 --  |
  |
  | --- frag NX_WIN..2*NX_WIN-1 -->  |
  |                                   | store, ack(bitmap, base=NX_WIN)
  | <----- NxAck base=NX_WIN -----   |
  ...
```

If an ACK does not arrive within `NX_TIMEOUT_MS`, the window is retransmitted up to `NX_RETRY` times.

---

## Configuration

Override any macro **before** including `NxProtocol.h` (or via `build_flags`).

| Macro            | Default | Meaning                                      |
| ---------------- | ------- | -------------------------------------------- |
| `NX_PAY`         | 240     | Payload bytes per fragment                   |
| `NX_WIN`         | 16      | Fragments per ACK window (max 32)            |
| `NX_POOL`        | 128     | Fragment buffer pool size (≥ NX_MSG × NX_WIN)|
| `NX_MSG`         | 8       | Concurrent reassembly slots                  |
| `NX_TIMEOUT_MS`  | 300     | Per-window ACK wait (ms)                     |
| `NX_RETRY`       | 5       | Retransmit attempts per window               |
| `NX_MAX_BLOCKS`  | 256     | Max fragments per message (≈ 60 KB)          |
| `NX_RXQ`         | 8       | Receive queue depth (must be power of two)   |
| `NX_DUP`         | 16      | Per-peer dedup history depth                 |
| `NX_TTL`         | 5000    | Stale reassembly slot GC timeout (ms)        |

Compile-time assertions enforce:
- `NX_WIN  ≤ 32` (ACK bitmap is `uint32_t`)
- `NX_POOL ≥ NX_MSG × NX_WIN` (pool cannot starve)
- `NX_RXQ` is a power of two
- Target is little-endian

---

## Testing

Host-side unit tests run under PlatformIO's `native` environment using Unity:

```
pio test -e native
```

The protocol core (`NxProtocol`) takes an `ITransport` interface; tests use the in-memory `NxLoopback` transport (in `src/NxLoopback.h`) to exercise:

- Fragmentation and reassembly at various sizes
- Windowed ACK with multi-window messages
- ACK loss and retry recovery (20 % simulated drop)
- Obfuscation round-trip
- Dedup suppression
- GC timeout — stale reassembly slot reclamation
- RX queue full — overflow and drain/recovery
- Message RAII — slot release via destructor
- CRC, MAC parsing, logger

`NxLoopback.h` is in `src/` (guarded by `#ifdef NOWX_HOST_BUILD`) so you can use it in your own integration tests without copying files.

---

## Known limitations

- No flow control between concurrent senders to the same receiver.
- `NX_OBFUSCATE` is not cryptography.
- `send()` is synchronous and may block for up to `NX_TIMEOUT_MS × NX_RETRY × ⌈len / (NX_WIN × NX_PAY)⌉` ms.
- Maximum message size is `NX_MAX_BLOCKS × NX_PAY` (default ≈ 60 KB).

---

## License

MIT — see `LICENSE`.
