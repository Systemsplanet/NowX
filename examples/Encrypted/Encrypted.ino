// Demonstrates NX_ENCRYPT (AES-256-GCM).
// Both sender and receiver must call setKey() with the same 32-byte key
// before exchanging encrypted messages.

#include <NowX.h>

NowX now("tx-enc");

static const uint8_t KEY[32] = {
  0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,
  0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,0x10,
  0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,
  0x19,0x1A,0x1B,0x1C,0x1D,0x1E,0x1F,0x20
};

void setup() {
  Serial.begin(115200);
  now.begin();
  now.setPeer("AA:BB:CC:DD:EE:FF");
  now.setKey(KEY, sizeof(KEY));
}

void loop() {
  bool ok = now.send(String("secret payload"), NX_ENCRYPT);
  Serial.printf("send -> %s\n", ok ? "ok" : "FAIL");
  delay(2000);
}
