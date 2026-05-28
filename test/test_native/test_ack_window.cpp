#include <unity.h>
#include "NxProtocol.h"
#include "LoopbackTransport.h"
#include <vector>
#include <cstdlib>

using namespace nowx;

static void ack_multiwindow_succeeds() {
  LoopbackBus bus;
  uint8_t macA[6] = {2,0,0,0,0,1};
  uint8_t macB[6] = {2,0,0,0,0,2};
  LoopbackEndpoint epA(&bus, macA);
  LoopbackEndpoint epB(&bus, macB);
  NxProtocol pa(&epA), pb(&epB);
  pa.setPeer(macB);
  pb.setPeer(macA);

  size_t sz = (size_t)NX_PAY * (NX_WIN * 3 + 5);
  std::vector<uint8_t> payload(sz);
  for (size_t i = 0; i < sz; i++) payload[i] = (uint8_t)(i & 0xFF);

  TEST_ASSERT_TRUE(pa.send(payload.data(), (uint32_t)sz));

  Message m;
  TEST_ASSERT_TRUE(pb.receive(m));
  TEST_ASSERT_EQUAL_UINT32((uint32_t)sz, m.len());
  TEST_ASSERT_EQUAL_INT8_ARRAY(payload.data(), m.data(), sz);
  m.release();
}

static void ack_recovers_from_loss() {
  srand(42);
  LoopbackBus bus;
  bus.dropPercent = 20;

  uint8_t macA[6] = {3,0,0,0,0,1};
  uint8_t macB[6] = {3,0,0,0,0,2};
  LoopbackEndpoint epA(&bus, macA);
  LoopbackEndpoint epB(&bus, macB);
  NxProtocol pa(&epA), pb(&epB);
  pa.setPeer(macB);
  pb.setPeer(macA);

  std::vector<uint8_t> payload(1000);
  for (size_t i = 0; i < payload.size(); i++) payload[i] = (uint8_t)i;

  bool ok = pa.send(payload.data(), (uint32_t)payload.size());
  for (int attempt = 0; attempt < 5 && !ok; attempt++)
    ok = pa.send(payload.data(), (uint32_t)payload.size());
  TEST_ASSERT_TRUE(ok);

  Message m;
  TEST_ASSERT_TRUE(pb.receive(m));
  TEST_ASSERT_EQUAL_INT8_ARRAY(payload.data(), m.data(), payload.size());
  m.release();
}

void run_ack_window_tests() {
  RUN_TEST(ack_multiwindow_succeeds);
  RUN_TEST(ack_recovers_from_loss);
}
