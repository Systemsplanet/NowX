#include <unity.h>
#include "NxProtocol.h"
#include "LoopbackTransport.h"
#include <cstring>

using namespace nowx;

static void xor_involution() {
  LoopbackBus bus;
  uint8_t mac[6] = {1,2,3,4,5,6};
  LoopbackEndpoint ep(&bus, mac);
  NxProtocol p(&ep);

  uint8_t key[8] = {0xAA,0x55,0x12,0x34,0x56,0x78,0x9A,0xBC};
  p.setKey(key, sizeof(key));

  uint8_t buf[100], orig[100];
  for (int i = 0; i < 100; i++) buf[i] = (uint8_t)i;
  std::memcpy(orig, buf, 100);

  p.xorInPlace(buf, 100);
  TEST_ASSERT_EQUAL_INT(0, std::memcmp(buf, orig, 100) == 0 ? 0 : 1);
  p.xorInPlace(buf, 100);
  TEST_ASSERT_EQUAL_INT8_ARRAY(orig, buf, 100);
}

static void xor_disabled_is_noop() {
  LoopbackBus bus;
  uint8_t mac[6] = {1,2,3,4,5,6};
  LoopbackEndpoint ep(&bus, mac);
  NxProtocol p(&ep);

  uint8_t buf[16], orig[16];
  for (int i = 0; i < 16; i++) buf[i] = (uint8_t)(i * 7);
  std::memcpy(orig, buf, 16);
  p.xorInPlace(buf, 16);
  TEST_ASSERT_EQUAL_INT8_ARRAY(orig, buf, 16);
}

void run_xor_tests() {
  RUN_TEST(xor_involution);
  RUN_TEST(xor_disabled_is_noop);
}
