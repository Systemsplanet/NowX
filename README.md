# NowX

Reliable, fragmented messaging over **ESP-NOW** for ESP32.

NowX wraps the raw ~250-byte ESP-NOW frame with:

- **Fragmentation & reassembly** of arbitrary-size messages (up to `NX_MAX_BYTES`, default 60 KB).
- **Windowed ACKs with retries** for delivery reliability.
- **CRC32** integrity check per fragment.
- **Optional XOR obfuscation** of payloads (see security note below).
- A **host-testable protocol core** (`NxProtocol`) decoupled from Arduino/ESP-IDF, so the bulk of the logic runs under Unity on your laptop.

> ⚠️ **Security note:** `NX_OBFUSCATE` (formerly `NX_ENCRYPT`) is XOR with a repeating key. It is **not** cryptography. Use it to discourage casual sniffing only. For real confidentiality, configure ESP-NOW's built-in LMK/PMK or layer AES-GCM on top.

---

## Install

### PlatformIO

```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
lib_deps = file:///path/to/nowx ; or a git URL
```

### Arduino IDE

Copy this folder into `~/Arduino/libraries/NowX/`.

---

## Quick start

### Receiver

```cpp
#include <NowX.h>

NowX now("rx");

void setup() {
  Serial.begin(115200);
  now.begin();
}

void loop() {
  Message m;
  if (now.receive(m)) {
    Serial.printf("got %u bytes: %s\n", m.len(), m.str().c_str());
    m.release(); // hand the reassembly buffer back to the pool
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
  now.send(String("hello"));
  delay(1000);
}
```

See `examples/` for `Sender`, `Receiver`, and `Encrypted` sketches.

---

## Wire format

All multi-byte fields are little-endian (ESP32 native).

```
NxHdr (17 bytes, packed):
  uint32_t magic = 0x4E585031 ('NXP1')
  uint32_t msg   monotonically increasing message id
  uint16_t blk   block index within this message
  uint16_t total total blocks in this message
  uint16_t len   payload length in this frame (<= NX_PAY)
  uint8_t  flags NX_ACK | NX_OBFUSCATE | ...
  uint32_t crc   CRC32 of on-wire payload bytes

Followed by `len` payload bytes (possibly obfuscated).

NxAck (16 bytes, packed):
  uint32_t msg   message being acked
  uint32_t base  window base block index this ack covers
  uint32_t map   bitmap of received blocks within this window
  uint32_t ts    sender timestamp (informational)
```

CRC is computed on the **on-wire** bytes, so a receiver validates CRC before any deobfuscation.

---

## Sliding-window ACK

```
sender                          receiver
  | --- blk 0..NX_WIN-1 --->  |
  |                            | store, ack(map, base=0)
  | <-------- NxAck base=0 -- |
  |
  | --- blk NX_WIN..2*NX_WIN-1 --> |
  |                                 | store, ack(map, base=NX_WIN)
  | <----- NxAck base=NX_WIN ----- |
  ...
```

If an ACK does not arrive within `NX_TIMEOUT` ms, the window is retransmitted up to `NX_RETRY` times.

---

## Configuration

| Macro          | Default  | Meaning                                |
|----------------|----------|----------------------------------------|
| `NX_PAY`       | 240      | Bytes of payload per fragment          |
| `NX_WIN`       | 16       | Fragments per ACK window               |
| `NX_POOL`      | 64       | Segment buffer pool size               |
| `NX_MSG`       | 8        | Concurrent reassembly slots            |
| `NX_TIMEOUT`   | 300 ms   | Per-window ACK timeout                 |
| `NX_RETRY`     | 5        | Retries per window                     |
| `NX_MAX_BLOCKS`| 256      | Max blocks per message (≈60 KB)        |
| `NX_RX_QUEUE`  | 8        | Receive queue depth                    |
| `NX_DEDUP`     | 16       | Per-peer recent-msg dedup history      |
| `NX_ASM_TTL`   | 5000 ms  | Reassembly slot timeout (GC)           |

---

## Testing

Host-side unit tests run under PlatformIO's `native` environment using Unity:

```bash
pio test -e native
```

The protocol core (`NxProtocol`) takes a transport interface; tests use an in-memory `LoopbackTransport` to exercise fragmentation, reassembly, ACK loss, retries, encryption round-trip, and duplicate suppression — all without flashing hardware.

---

## Known limitations

- No flow control between concurrent senders to the same receiver.
- `NX_OBFUSCATE` is not cryptography.
- Max message size is `NX_MAX_BLOCKS * NX_PAY` (default ≈ 60 KB).

---

## License

MIT — see `LICENSE`.
