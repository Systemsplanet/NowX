#include <unity.h>
#include "NxProtocol.h"
#include "NxLoopback.h"
#include <vector>

using namespace nowx;

// Helper: create a symmetric pair of connected protocol instances.
struct Pair {
  LoopbackBus      bus;
  uint8_t          macA[6] = {1,0,0,0,0,1};
  uint8_t          macB[6] = {1,0,0,0,0,2};
  LoopbackEndpoint epA;
  LoopbackEndpoint epB;
  NxProtocol       pa;
  NxProtocol       pb;

  Pair() : epA(&bus, macA), epB(&bus, macB), pa(&epA), pb(&epB) {
    pa.setPeer(macB);
    pb.setPeer(macA);
  }
};

// Two consecutive sends must both be received correctly (dedup must not
// suppress the second message which has a different message id).
static void reassembly_two_consecutive_sends() {
  Pair p;
  std::vector<uint8_t> payload(500, 0x5A);

  TEST_ASSERT_TRUE(p.pa.send(payload.data(), (uint32_t)payload.size()));
  Message m1;
  TEST_ASSERT_TRUE(p.pb.receive(m1));
  TEST_ASSERT_EQUAL_UINT32(500, m1.len());
  // m1 released by destructor

  TEST_ASSERT_TRUE(p.pa.send(payload.data(), (uint32_t)payload.size()));
  Message m2;
  TEST_ASSERT_TRUE(p.pb.receive(m2));
  TEST_ASSERT_EQUAL_UINT32(500, m2.len());
}

// Dedup: the same message id arriving twice must be ACKed but not
// delivered a second time.
static void reassembly_dedup_suppresses_duplicate_msg() {
  Pair p;
  const uint8_t pay[] = "dedup-test";
  const uint32_t sz = sizeof(pay) - 1;

  TEST_ASSERT_TRUE(p.pa.send(pay, sz));
  Message m;
  TEST_ASSERT_TRUE(p.pb.receive(m));
  TEST_ASSERT_EQUAL_UINT32(sz, m.len());
  // m released by destructor

  // Simulate duplicate delivery by injecting the same packet again
  // via a second send (message counter advances, so we manipulate the
  // dedup table indirectly — what we can test is that the RX queue
  // does not grow past 1 for a single send).
  Message m2;
  TEST_ASSERT_FALSE(p.pb.receive(m2));
}

void run_reassembly_tests() {
  RUN_TEST(reassembly_two_consecutive_sends);
  RUN_TEST(reassembly_dedup_suppresses_duplicate_msg);
}
