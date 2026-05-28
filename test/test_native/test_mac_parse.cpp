#include <unity.h>
#include "NxProtocol.h"

using namespace nowx;

static void mac_valid() {
  uint8_t m[6] = {0};
  TEST_ASSERT_TRUE(NxProtocol::parseMac("AA:BB:CC:DD:EE:FF", m));
  TEST_ASSERT_EQUAL_UINT8(0xAA, m[0]);
  TEST_ASSERT_EQUAL_UINT8(0xFF, m[5]);
}

static void mac_lowercase() {
  uint8_t m[6] = {0};
  TEST_ASSERT_TRUE(NxProtocol::parseMac("aa:bb:cc:dd:ee:ff", m));
  TEST_ASSERT_EQUAL_UINT8(0xCC, m[2]);
}

static void mac_invalid_short() {
  uint8_t m[6] = {0};
  TEST_ASSERT_FALSE(NxProtocol::parseMac("AA:BB:CC", m));
}

static void mac_invalid_null() {
  uint8_t m[6] = {0};
  TEST_ASSERT_FALSE(NxProtocol::parseMac(nullptr, m));
}

static void mac_out_of_range() {
  uint8_t m[6] = {0};
  TEST_ASSERT_FALSE(NxProtocol::parseMac("1FF:BB:CC:DD:EE:FF", m));
}

void run_mac_tests() {
  RUN_TEST(mac_valid);
  RUN_TEST(mac_lowercase);
  RUN_TEST(mac_invalid_short);
  RUN_TEST(mac_invalid_null);
  RUN_TEST(mac_out_of_range);
}
