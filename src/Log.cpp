#include "Log.h"
#include <stdio.h>
#include <string.h>

#ifdef NOWX_HOST_BUILD
namespace nowx_host {
  static uint32_t (*g_millis_fn)() = nullptr;
  static FILE *g_stream = nullptr;

  void set_millis(uint32_t (*fn)()) { g_millis_fn = fn; }
  uint32_t millis() { return g_millis_fn ? g_millis_fn() : 0; }
  void set_log_stream(FILE *f) { g_stream = f; }
  FILE *log_stream() { return g_stream ? g_stream : stderr; }
}
#endif

namespace nowx {

#ifndef NOWX_HOST_BUILD
  static Stream *g_stream = &Serial;
  void setLogStream(Stream *s) { g_stream = s; }
#endif

  void formatTimestamp(char *b, size_t n, uint32_t millis_now) {
    if (n < 7) {
      if (n) b[0] = '\0';
      return;
    }
    uint32_t t = millis_now / 1000u;
    uint8_t h = (uint8_t)((t / 3600u) % 24u);
    uint8_t m = (uint8_t)((t / 60u) % 60u);
    uint8_t s = (uint8_t)(t % 60u);
    snprintf(b, n, "%02u%02u%02u",
             (unsigned)h, (unsigned)m, (unsigned)s);
  }

  static uint32_t now_ms() {
#ifdef NOWX_HOST_BUILD
    return nowx_host::millis();
#else
    return ::millis();
#endif
  }

  static void write_line(const char *ts, const char *line) {
#ifdef NOWX_HOST_BUILD
    fprintf(nowx_host::log_stream(), "%s %s\n", ts, line);
#else
    if (!g_stream) return;
    g_stream->print(ts);
    g_stream->print(' ');
    g_stream->println(line);
#endif
  }

  void logln(const char *s) {
    char ts[8];
    formatTimestamp(ts, sizeof(ts), now_ms());
    write_line(ts, s ? s : "");
  }

  void logf(const char *fmt, ...) {
    char ts[8];
    formatTimestamp(ts, sizeof(ts), now_ms());

    char buf[256];
    va_list a;
    va_start(a, fmt);
    int written = vsnprintf(buf, sizeof(buf), fmt, a);
    va_end(a);
    (void)written;

    write_line(ts, buf);
  }
}
