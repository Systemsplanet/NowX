#include "Log.h"
#include <stdarg.h>

static void _ts(char *b) {
  uint32_t t = millis() / 1000;

  uint8_t h = (t / 3600) % 24;
  uint8_t m = (t / 60) % 60;
  uint8_t s = t % 60;

  sprintf(b, "%02u%02u%02u", h,m,s);
}

void logln(const char *s) {

  char t[16];

  _ts(t);

  Serial.print(t);
  Serial.print(" ");
  Serial.println(s);
}

void logf(const char *fmt, ...) {

  char t[16];
  char b[256];

  _ts(t);

  va_list a;

  va_start(a, fmt);

  vsnprintf(b, sizeof(b), fmt, a);

  va_end(a);

  Serial.print(t);
  Serial.print(" ");
  Serial.println(b);
}
