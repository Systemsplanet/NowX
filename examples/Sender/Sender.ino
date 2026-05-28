#include <NowX.h>

NowX now("tx");

void setup() {
  Serial.begin(115200);
  delay(200);
  if (!now.begin()) {
    Serial.println("ESP-NOW init failed");
    while (true) delay(1000);
  }
  // Replace with your receiver's MAC.
  now.setPeer("AA:BB:CC:DD:EE:FF");
}

void loop() {
  static uint32_t n = 0;
  String s = String("hello #") + n++;
  bool ok = now.send(s);
  Serial.printf("send -> %s\n", ok ? "ok" : "FAIL");
  delay(1000);
}
