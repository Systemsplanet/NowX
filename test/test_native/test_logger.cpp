#include <unity.h>
#include "Log.h"
#include <cstring>
#include <cstdio>

static uint32_t g_fake_ms = 0;
static uint32_t fake_millis() { return g_fake_ms; }

static void ts_zero() {
  char b[8];
  nowx::formatTimestamp(b, sizeof(b), 0);
  TEST_ASSERT_EQUAL_STRING("000000", b);
}

static void ts_one_minute() {
  char b[8];
  nowx::formatTimestamp(b, sizeof(b), 60u * 1000u);
  TEST_ASSERT_EQUAL_STRING("000100", b);
}

static void ts_one_hour() {
  char b[8];
  nowx::formatTimestamp(b, sizeof(b), 3600u * 1000u);
  TEST_ASSERT_EQUAL_STRING("010000", b);
}

static void ts_wrap_24h() {
  char b[8];
  nowx::formatTimestamp(b, sizeof(b), (24u * 3600u + 1u) * 1000u);
  TEST_ASSERT_EQUAL_STRING("000001", b);
}

static void logf_writes_to_stream() {
  FILE *f = tmpfile();
  TEST_ASSERT_NOT_NULL(f);
  nowx_host::set_log_stream(f);
  nowx_host::set_millis(fake_millis);
  g_fake_ms = 3661u * 1000u;

  nowx::logf("hello %d", 42);
  fflush(f);
  rewind(f);

  char buf[64] = {0};
  size_t n = fread(buf, 1, sizeof(buf) - 1, f);
  buf[n] = '\0';
  fclose(f);
  nowx_host::set_log_stream(nullptr);

  TEST_ASSERT_NOT_NULL(strstr(buf, "010101"));
  TEST_ASSERT_NOT_NULL(strstr(buf, "hello 42"));
}

void run_logger_tests() {
  RUN_TEST(ts_zero);
  RUN_TEST(ts_one_minute);
  RUN_TEST(ts_one_hour);
  RUN_TEST(ts_wrap_24h);
  RUN_TEST(logf_writes_to_stream);
}
