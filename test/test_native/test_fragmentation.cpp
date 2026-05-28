#include <unity.h>
#include "NxProtocol.h"
#include "LoopbackTransport.h"
#include <vector>
#include <cstring>

using namespace nowx;

static void roundtrip(size_t size) {
  LoopbackBus bus;
  uint8_t macA[6] = {0xA,0,0,0,0,1};
  uint8_t macB[6] = {0xB,0,0,0,0,2};
  LoopbackEndpoint epA(&bus, macA);
  LoopbackEndpoint epB(&bus, macB);
  NxProtocol pa(&epA), pb(&epB);
  pa.setPeer(macB);
  pb.setPeer(macA);

  std::vector<uint8_t> payload(size);
  for (size_t i = 0; i < size; i++) payload[i] = (uint8_t)(i * 31 + 7);

  TEST_ASSERT_TRUE(pa.send(payload.data(), (uint32_t)size));

  Message m;
  TEST_ASSERT_TRUE(pb.receive(m));
  TEST_ASSERT_EQUAL_UINT32((uint32_t)size, m.len());
  TEST_ASSERT_EQUAL_INT8_ARRAY(payload.data(), m.data(), size);
  m.release();
}

static void frag_single_block()  { roundtrip(10); }
static void frag_exact_block()   { roundtrip(NX_PAY); }
static void frag_block_plus_one(){ roundtrip(NX_PAY + 1); }
static void frag_full_window()   { roundtrip(NX_PAY * NX_WIN); }
static void frag_two_windows()   { roundtrip(NX_PAY * (NX_WIN + 3)); }
static void frag_large()         { roundtrip(NX_PAY * 50); }

void run_fragmentation_tests() {
  RUN_TEST(frag_single_block);
  RUN_TEST(frag_exact_block);
  RUN_TEST(frag_block_plus_one);
  RUN_TEST(frag_full_window);
  RUN_TEST(frag_two_windows);
  RUN_TEST(frag_large);
}
