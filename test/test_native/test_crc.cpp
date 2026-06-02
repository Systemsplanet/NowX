#include <unity.h>
#include "NxProtocol.h"
#include <cstring>

using namespace nowx;

static void crc_empty() {
  // crc32(nullptr, 0) must return 0 — not 0xFFFFFFFF or ~0.
  TEST_ASSERT_EQUAL_UINT32(0x00000000u, NxProtocol::crc32(nullptr, 0));
}

static void crc_known_vector() {
  // Well-known CRC-32/ISO-HDLC test vector.
  const char *s = "123456789";
  uint32_t c = NxProtocol::crc32(reinterpret_cast<const uint8_t*>(s), 9);
  TEST_ASSERT_EQUAL_UINT32(0xCBF43926u, c);
}

static void crc_single_byte_changes() {
  uint8_t a[4] = {1,2,3,4};
  uint8_t b[4] = {1,2,3,5};
  TEST_ASSERT_NOT_EQUAL(NxProtocol::crc32(a, 4), NxProtocol::crc32(b, 4));
}

void run_crc_tests() {
  RUN_TEST(crc_empty);
  RUN_TEST(crc_known_vector);
  RUN_TEST(crc_single_byte_changes);
}
