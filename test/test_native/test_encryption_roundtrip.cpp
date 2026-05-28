#include <unity.h>
#include "NxProtocol.h"
#include "LoopbackTransport.h"
#include <vector>
#include <cstring>

using namespace nowx;

static void obfuscation_roundtrip() {
  LoopbackBus bus;
  uint8_t macA[6] = {9,0,0,0,0,1};
  uint8_t macB[6] = {9,0,0,0,0,2};
  LoopbackEndpoint epA(&bus, macA);
  LoopbackEndpoint epB(&bus, macB);
  NxProtocol pa(&epA), pb(&epB);
  pa.setPeer(macB);
  pb.setPeer(macA);

  uint8_t key[16];
  for (int i = 0; i < 16; i++) key[i] = (uint8_t)(0x11 * i + 1);
  pa.setKey(key, 16);
  pb.setKey(key, 16);

  std::vector<uint8_t> payload(600);
  for (size_t i = 0; i < payload.size(); i++) payload[i] = (uint8_t)(i ^ 0xA5);

  TEST_ASSERT_TRUE(pa.send(payload.data(), (uint32_t)payload.size(), NX_OBFUSCATE));

  Message m;
  TEST_ASSERT_TRUE(pb.receive(m));
  TEST_ASSERT_EQUAL_UINT32((uint32_t)payload.size(), m.len());
  TEST_ASSERT_TRUE(m.obfuscated());
  TEST_ASSERT_EQUAL_INT8_ARRAY(payload.data(), m.data(), payload.size());
  m.release();
}

void run_encryption_roundtrip_tests() {
  RUN_TEST(obfuscation_roundtrip);
}
