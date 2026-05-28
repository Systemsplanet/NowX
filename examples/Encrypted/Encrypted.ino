// Demonstrates NX_OBFUSCATE.
// WARNING: This is XOR, not cryptography. See README.

#include <NowX.h>

NowX now("tx-obf");

static const uint8_t KEY[16] = {
  0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,
  0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,0x10
};

void setup() {
  Serial.begin(115200);
  now.begin();
  now.setPeer("AA:BB:CC:DD:EE:FF");
  now.setKey(KEY, sizeof(KEY));
}

void loop() {
  now.send(String("secretish payload"), NX_OBFUSCATE);
  delay(2000);
}
