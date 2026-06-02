# NowX

Reliable, fragmented messaging over **ESP-NOW** for ESP32.

NowX wraps the raw ~250-byte ESP-NOW frame with:

- **Fragmentation & reassembly** of messages up to `NX_MAX_PAYLOAD_BYTES` (default **10 MB**).
- **Windowed ACKs with retries** for delivery reliability.
- **CRC32** integrity check per fragment.
- **AES-256-GCM** authenticated encryption (optional; see `examples/Encrypted`).
- A **host-testable protocol core** (`NxProtocol`) decoupled from Arduino/ESP-IDF, so the bulk of the logic runs under Unity on your laptop.

---

## ⚠️ Security

The optional AES-256-GCM path (`NX_ENCRYPT` flag / `setKey()`) provides authenticated encryption. The software fallback compiled in for `NOWX_HOST_BUILD` is **not constant-time** and must not be used in production — the on-device path uses the ESP32's hardware-accelerated mbedTLS.

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
  delay(200);
  if (!now.begin()) {
    Serial.println("ESP-NOW init failed");
    while (true) delay(1000);
  }
  Serial.print("MAC: ");
  Serial.println(WiFi.macAddress());
}

void loop() {
  Message m;
  if (now.receive(m)) {
    Serial.printf("got msg=%u len=%u: ",
                  (unsigned)m.id(), (unsigned)m.len());
    Serial.write(m.data(), m.len());
    Serial.println();
    // m.release() is called automatically when m goes out of scope.
  }
  delay(5);
}
```

### Sender

```cpp
#include <NowX.h>

NowX now("tx");

void setup() {
  Serial.begin(115200);
  delay(200);
  if (!now.begin()) {
    Serial.println("ESP-NOW init failed");
    while (true) delay(1000);
  }
  // Replace with your receiver's MAC address.
  now.setPeer("AA:BB:CC:DD:EE:FF");
}

void loop() {
  static uint32_t n = 0;
  String s = String("hello #") + n++;
  bool ok = now.send(s);
  Serial.printf("send -> %s\n", ok ? "ok" : "FAIL");
  delay(1000);
}
```

See `examples/` for `Sender`, `Receiver`, and `Encrypted` sketches.

---

## NowX API

All names live in the `nowx::` namespace. The `NowX.h` header pulls the most-used ones into the global namespace for sketch convenience:

```cpp
using nowx::NowX;
using nowx::Message;
using nowx::NX_ENCRYPT;
using nowx::NX_ACK;
```

### `NowX`

```cpp
NowX(const char *name);          // name is used only for log output
bool begin();                    // init WiFi station + ESP-NOW; call once in setup()
void setPeer(const char *mac);   // set remote peer — "AA:BB:CC:DD:EE:FF" format
void setKey(const uint8_t *key, uint32_t len);  // AES-256-GCM key; nullptr/0 disables
bool send(const uint8_t *d, uint32_t len, uint8_t flags = 0);
bool send(const String &s,               uint8_t flags = 0);
bool receive(Message &m);        // pop one fully-reassembled message; false = nothing ready
NxProtocol& protocol();          // access the underlying engine for advanced use
```

`send()` is synchronous; it blocks until all windows are ACKed or all retries are exhausted. Pass `NX_ENCRYPT` in `flags` to encrypt the payload with AES-256-GCM (requires a key set via `setKey()`).

### `Message`

`Message` is an RAII handle to a reassembled payload. It is non-copyable; move-only.

```cpp
uint8_t  *data()      const;  // pointer to payload bytes
uint32_t  len()       const;  // payload length in bytes
uint32_t  id()        const;  // monotonically increasing message ID
bool      encrypted() const;  // true if NX_ENCRYPT flag was set
std::string str()     const;  // payload as std::string
void      release();          // free the slot early; also called by destructor
```

---

## Wire format (v2)

All multi-byte fields are little-endian (ESP32 native).

> **v2 change:** `idx` and `cnt` are now `uint32_t` (were `uint16_t`) to
> support messages with more than 65 535 fragments. The magic value changed
> from `0x4E585031` (`NXP1`) to `0x4E585032` (`NXP2`) so v1 peers reject
> v2 frames gracefully.

```
NxHdr (23 bytes, packed):
  uint32_t magic  = 0x4E585032 ('NXP2')
  uint32_t msg    monotonically increasing message id
  uint32_t idx    fragment index within this message   [v2: was uint16_t]
  uint32_t cnt    total fragment count                 [v2: was uint16_t]
  uint16_t pay    on-wire payload bytes (<= NX_ENC_PAY)
  uint8_t  flags  NX_ENCRYPT | ...
  uint32_t crc    CRC32 of on-wire payload bytes

Followed by `pay` payload bytes (plain or IV||ciphertext||tag).

NxAck (17 bytes, packed):
  uint8_t  type   NX_IS_ACK (0x80) — distinguishes ACK frames from data
  uint32_t msg    message being acked
  uint32_t base   window base fragment index this ack covers
  uint32_t ack    bitmap of received fragments within this window
  uint32_t t      sender timestamp (informational, ms)
```

CRC is computed on the **on-wire** bytes (post-encryption if applicable).

ACK frames are distinguished from data frames by the `type` field at byte 0, not by packet length alone.

### Frame flags

| Flag          | Value  | Meaning                                      |
| ------------- | ------ | -------------------------------------------- |
| `NX_ACK`      | `0x01` | Reserved for future per-fragment ACK request |
| `NX_ENCRYPT`  | `0x02` | Payload is `IV \|\| ciphertext \|\| GCM-tag` |
| `NX_COMPRESS` | `0x04` | Reserved                                     |
| `NX_STREAM`   | `0x08` | Reserved                                     |
| `NX_PRIO`     | `0x10` | Reserved                                     |
| `NX_IS_ACK`   | `0x80` | Frame is an `NxAck` (not data)               |

Only `NX_ENCRYPT` is currently implemented.

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

## Memory model

NowX v2 uses **heap-allocated, demand-sized buffers** throughout:

| Structure | Old (v1) | New (v2) |
|-----------|----------|----------|
| Fragment data buffer | `uint8_t data[NX_ENC_PAY]` (static, always 268 B) | `malloc`'d to actual fragment size |
| Reassembly frag[] pointer array | `Frag*[NX_MAX_BLOCKS]` (static, 512 B per slot) | `malloc`'d to `cnt` entries on first fragment |
| RX message buffer | `uint8_t buf[NX_MAX_BYTES]` (static, up to 10 MB per slot!) | `malloc`'d to exact reassembled length |

This means the pool default (`NX_POOL 32`) and reassembly slots (`NX_MSG 4`) are much smaller than v1 without reducing throughput: memory is only consumed for fragments actually in flight.

**ESP32 heap tip:** For 10 MB messages you need sufficient heap. The ESP32's default heap is 300–520 KB; for large transfers consider using PSRAM (`-DBOARD_HAS_PSRAM`) and a custom allocator, or keep message sizes within available DRAM.

---

## Configuration

Override any macro **before** including `NxProtocol.h` (or via `build_flags` in `platformio.ini`).

| Macro                  | Default    | Meaning                                              |
| ---------------------- | ---------- | ---------------------------------------------------- |
| `NX_PAY`               | 240        | Plaintext payload bytes per fragment                 |
| `NX_WIN`               | 16         | Fragments per ACK window (max 32)                    |
| `NX_POOL`              | 32         | Fragment pool slots (buffers allocated on demand)    |
| `NX_MSG`               | 4          | Concurrent reassembly slots                          |
| `NX_TIMEOUT_MS`        | 300        | Per-window ACK wait (ms)                             |
| `NX_RETRY`             | 5          | Retransmit attempts per window                       |
| `NX_MAX_BLOCKS`        | 43690      | Max fragments per message (floor(10 MB / NX_PAY))    |
| `NX_RXQ`               | 4          | Receive queue depth (must be power of two)           |
| `NX_DUP`               | 16         | Per-peer dedup history depth                         |
| `NX_TTL`               | 5000       | Stale reassembly slot GC timeout (ms)                |
| `NX_MAX_PAYLOAD_BYTES` | 10 485 760 | Hard ceiling on message size (10 MB)                 |

Derived constants (not overridable):

| Constant      | Value                            | Meaning                          |
| ------------- | -------------------------------- | -------------------------------- |
| `NX_GCM_IV_LEN`  | 12                            | GCM IV size in bytes             |
| `NX_GCM_TAG_LEN` | 16                            | GCM authentication tag size      |
| `NX_ENC_PAY`  | `NX_PAY + 12 + 16`              | Max on-wire bytes per fragment   |

Compile-time assertions enforce:
- `NX_WIN  ≤ 32` (ACK bitmap is `uint32_t`)
- `NX_POOL ≥ 1`
- `NX_RXQ` is a power of two
- Target is little-endian
- `NX_MAX_BLOCKS × NX_PAY ≤ NX_MAX_PAYLOAD_BYTES`

---

## Architecture

```
NowX              (Arduino facade — NowX.h / NowX.cpp)
  └─ NxProtocol   (protocol engine — NxProtocol.h / NxProtocol.cpp)
       ├─ NxCrypto       — AES-256-GCM key management and per-fragment crypto
       ├─ NxAckEngine    — ACK store, wait, and send
       ├─ NxFragmenter   — TX windowed fragmentation with retries
       ├─ NxReassembler  — RX sliding-window reassembly, dedup, GC
       └─ NxQueue        — Circular RX message queue (RAII slots)

ITransport        (abstract interface — ITransport.h)
  ├─ EspNowTransport   — on-device ESP-NOW backend (EspNowTransport.h/.cpp)
  └─ LoopbackEndpoint  — in-memory backend for tests (src/NxLoopback.h)
```

`NxProtocol` depends only on `ITransport`; it has no direct Arduino or ESP-IDF dependencies. This is what makes it host-testable.

### EspNowTransport

`EspNowTransport` is a **singleton per device**: only one instance may receive packets, because the ESP-NOW receive callback is a single module-level pointer. Creating a second instance overwrites the pointer and emits a warning. If you need a second logical channel, use a second ESP32.

### NxLoopback (in `src/NxLoopback.h`)

Available only when `NOWX_HOST_BUILD` is defined. Provides two classes:

- **`LoopbackBus`** — virtual clock (`now`, advanced with `bus.advance(ms)`) and configurable packet drop rate (`bus.dropPercent = 0–100`) for exercising retry/ACK paths.
- **`LoopbackEndpoint`** — one node on the bus; implements `ITransport`. Construct with a `LoopbackBus*` and a 6-byte MAC.

```cpp
LoopbackBus bus;
bus.dropPercent = 20;          // drop 20 % of packets to exercise retries

uint8_t macA[6] = {2,0,0,0,0,1};
uint8_t macB[6] = {2,0,0,0,0,2};
LoopbackEndpoint epA(&bus, macA);
LoopbackEndpoint epB(&bus, macB);

NxProtocol pa(&epA), pb(&epB);
pa.setPeer(macB);
pb.setPeer(macA);
```

The test directory includes `LoopbackTransport.h` as a backward-compatibility shim that simply includes `NxLoopback.h`.

---

## Testing

Host-side unit tests run under PlatformIO's `native` environment using Unity:

```
pio test -e native
```

The protocol core (`NxProtocol`) takes an `ITransport` interface; tests use `LoopbackEndpoint` / `LoopbackBus` from `src/NxLoopback.h` to exercise:

- Fragmentation and reassembly at various sizes
- Windowed ACK with multi-window messages
- ACK loss and retry recovery (20 % simulated drop via `LoopbackBus.dropPercent`)
- Encryption round-trip (AES-256-GCM)
- Dedup suppression
- GC timeout — stale reassembly slot reclamation
- RX queue full — overflow and drain/recovery
- Message RAII — slot release via destructor
- CRC, MAC parsing, logger, `Message::str()`

To run a single test group, edit `test/test_native/test_main.cpp` and comment out the others, or use:

```
pio test -e native --filter test_native
```

---

## Known limitations

- No flow control between concurrent senders to the same receiver.
- `send()` is synchronous and may block for up to `NX_TIMEOUT_MS × NX_RETRY × ⌈len / (NX_WIN × NX_PAY)⌉` ms.
- Maximum message size is `NX_MAX_PAYLOAD_BYTES` (default 10 MB).
- Large messages require sufficient heap (or PSRAM on ESP32).
- Only one `EspNowTransport` instance per device.

---

## License

MIT — see `LICENSE`.
