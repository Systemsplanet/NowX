#include <unity.h>
#include "NxProtocol.h"
#include "LoopbackTransport.h"
#include <string>

using namespace nowx;

static void str_with_embedded_nul() {
  LoopbackBus bus;
  uint8_t macA[6] = {7,0,0,0,0,1};
  uint8_t macB[6] = {7,0,0,0,0,2};
  LoopbackEndpoint epA(&bus, macA);
  LoopbackEndpoint epB(&bus, macB);
  NxProtocol pa(&epA), pb(&epB);
  pa.setPeer(macB);
  pb.setPeer(macA);

  const uint8_t payload[] = {'a','b','\0','c','d'};
  TEST_ASSERT_TRUE(pa.send(payload, 5));

  Message m;
  TEST_ASSERT_TRUE(pb.receive(m));
  std::string s = m.str();
  TEST_ASSERT_EQUAL_UINT32(5, (uint32_t)s.size());
  TEST_ASSERT_EQUAL_UINT8('a', (uint8_t)s[0]);
  TEST_ASSERT_EQUAL_UINT8('\0', (uint8_t)s[2]);
  TEST_ASSERT_EQUAL_UINT8('d', (uint8_t)s[4]);
  m.release();
}

void run_message_str_tests() {
  RUN_TEST(str_with_embedded_nul);
}
