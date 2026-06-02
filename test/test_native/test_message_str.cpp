#include <unity.h>
#include "NxProtocol.h"
#include "NxLoopback.h"
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
  TEST_ASSERT_EQUAL_UINT8('a',  (uint8_t)s[0]);
  TEST_ASSERT_EQUAL_UINT8('\0', (uint8_t)s[2]);
  TEST_ASSERT_EQUAL_UINT8('d',  (uint8_t)s[4]);
  // m released by destructor
}

// RAII: forgetting release() must not leak the slot (destructor does it).
static void message_destructor_releases_slot() {
  LoopbackBus bus;
  uint8_t macA[6] = {8,0,0,0,0,1};
  uint8_t macB[6] = {8,0,0,0,0,2};
  LoopbackEndpoint epA(&bus, macA);
  LoopbackEndpoint epB(&bus, macB);
  NxProtocol pa(&epA), pb(&epB);
  pa.setPeer(macB);
  pb.setPeer(macA);

  const uint8_t pay[] = "slot-leak-test";

  // Fill and drain NX_RXQ times.  If the destructor doesn't release the
  // slot the queue stalls after the first round.
  for (int round = 0; round < NX_RXQ + 2; round++) {
    TEST_ASSERT_TRUE(pa.send(pay, sizeof(pay) - 1));
    {
      Message m; // goes out of scope without explicit release()
      TEST_ASSERT_TRUE(pb.receive(m));
    } // destructor fires here
  }
}

void run_message_str_tests() {
  RUN_TEST(str_with_embedded_nul);
  RUN_TEST(message_destructor_releases_slot);
}
