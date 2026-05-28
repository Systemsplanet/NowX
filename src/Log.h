#pragma once

// Log.h - tiny timestamped logger.
//
// On Arduino/ESP32 builds, output goes to a Stream (default: Serial).
// On host builds (NOWX_HOST_BUILD), output goes to a FILE* (default: stderr),
// and a fake millis() is provided so tests can control time.

#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>

#ifdef NOWX_HOST_BUILD
  #include <cstdio>
  namespace nowx_host {
    // Test hook: override the "millis" source.
    void set_millis(uint32_t (*fn)());
    uint32_t millis();
    void set_log_stream(FILE *f);
    FILE *log_stream();
  }
#else
  #include <Arduino.h>
#endif

namespace nowx {

#ifndef NOWX_HOST_BUILD
  // Redirect log output. Pass nullptr to disable.
  void setLogStream(Stream *s);
#endif

  // Format "HHMMSS" timestamp into b (must be >= 7 bytes).
  void formatTimestamp(char *b, size_t n, uint32_t millis_now);

  void logln(const char *s);
  void logf(const char *fmt, ...);
}

// Back-compat free-function shims.
inline void logln(const char *s) { nowx::logln(s); }
inline void logf(const char *fmt, ...) {
  va_list a;
  va_start(a, fmt);
  char b[256];
  vsnprintf(b, sizeof(b), fmt, a);
  va_end(a);
  nowx::logln(b);
}
