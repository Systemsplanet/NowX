#pragma once

// Log.h - tiny timestamped logger.
//
// On Arduino/ESP32 builds output goes to a Stream (default: Serial).
// On host builds (NOWX_HOST_BUILD) output goes to a FILE* (default: stderr),
// and a fake millis() source lets tests control time.
//
// All logging symbols live in the nowx:: namespace to avoid collisions
// with user code.  There are no free-function shims in the global namespace.

#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>

#ifdef NOWX_HOST_BUILD
  #include <cstdio>
  namespace nowx_host {
    void     set_millis(uint32_t (*fn)());
    uint32_t millis();
    void     set_log_stream(FILE *f);
    FILE    *log_stream();
  }
#else
  #include <Arduino.h>
#endif

namespace nowx {

#ifndef NOWX_HOST_BUILD
  // Redirect log output. Pass nullptr to silence all logging.
  void setLogStream(Stream *s);
#endif

  // Format an "HHMMSS" timestamp into b (must be >= 7 bytes).
  void formatTimestamp(char *b, size_t n, uint32_t ms_now);

  void logln(const char *s);
  void logf(const char *fmt, ...);

} // namespace nowx
