#include <NowX.h>

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
    m.release();
  }
  delay(5);
}
