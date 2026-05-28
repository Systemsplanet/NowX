#include <unity.h>
#include "NxProtocol.h"
#include "LoopbackTransport.h"
#include <vector>

using namespace nowx;

static void reassembly_dedup_on_retransmit() {
  LoopbackBus bus;
  uint8_t macA[6] = {1,0,0,0,0,1};
  uint8_t macB[6] = {1,0,0,0,0,2};
  LoopbackEndpoint epA(&bus, macA);
  LoopbackEndpoint epB(&bus, macB);
  NxProtocol pa(&epA), pb(&epB);
  pa.setPeer(macB);
  pb.setPeer(macA);

  std::vector<uint8_t> payload(500, 0x5A);
  TEST_ASSERT_TRUE(pa.send(payload.data(), (uint32_t)payload.size()));

  Message m;
  TEST_ASSERT_TRUE(pb.receive(m));
  TEST_ASSERT_EQUAL_UINT32(500, m.len());
  m.release();

  TEST_ASSERT_TRUE(pa.send(payload.data(), (uint32_t)payload.size()));
  TEST_ASSERT_TRUE(pb.receive(m));
  TEST_ASSERT_EQUAL_UINT32(500, m.len());
  m.release();
}

void run_reassembly_tests() {
  RUN_TEST(reassembly_dedup_on_retransmit);
}
