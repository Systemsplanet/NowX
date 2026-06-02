#include <unity.h>
#include "NxProtocol.h"
#include "NxLoopback.h"
#include <vector>
#include <cstring>

// Tests for NX_TTL — stale reassembly slot garbage collection.

using namespace nowx;

// If a partial message sits in a reassembly slot for longer than NX_TTL
// milliseconds the slot and all its fragment buffers must be reclaimed,
// so that subsequent complete messages can still be delivered.
static void gc_reclaims_timed_out_slot() {
  LoopbackBus bus;
  uint8_t macA[6] = {0xC,0,0,0,0,1};
  uint8_t macB[6] = {0xC,0,0,0,0,2};
  LoopbackEndpoint epA(&bus, macA);
  LoopbackEndpoint epB(&bus, macB);
  NxProtocol pa(&epA), pb(&epB);
  pa.setPeer(macB);
  pb.setPeer(macA);

  // Send a multi-fragment message but drop every packet so the
  // reassembly slot on pb fills partially and then stalls.
  bus.dropPercent = 100;
  std::vector<uint8_t> partial(NX_PAY * 3, 0xAB);
  // send() will fail (all retries exhausted) — that's expected.
  pa.send(partial.data(), (uint32_t)partial.size());

  // Advance virtual time past the GC TTL.
  bus.advance(NX_TTL + 1);
  bus.dropPercent = 0;

  // Now send a fresh complete message — it must arrive correctly.
  std::vector<uint8_t> good(100, 0x55);
  TEST_ASSERT_TRUE(pa.send(good.data(), (uint32_t)good.size()));

  Message m;
  TEST_ASSERT_TRUE(pb.receive(m));
  TEST_ASSERT_EQUAL_UINT32(100u, m.len());
  for (uint32_t i = 0; i < m.len(); i++) {
    TEST_ASSERT_EQUAL_UINT8(0x55, m.data()[i]);
  }
}

// GC must not leak fragment pool entries: after many timed-out partial
// messages the pool must still be usable.
static void gc_does_not_leak_pool_entries() {
  LoopbackBus bus;
  uint8_t macA[6] = {0xD,0,0,0,0,1};
  uint8_t macB[6] = {0xD,0,0,0,0,2};
  LoopbackEndpoint epA(&bus, macA);
  LoopbackEndpoint epB(&bus, macB);
  NxProtocol pa(&epA), pb(&epB);
  pa.setPeer(macB);
  pb.setPeer(macA);

  // Repeatedly send partial messages that time out.
  for (int iter = 0; iter < NX_MSG * 2; iter++) {
    bus.dropPercent = 100;
    std::vector<uint8_t> p(NX_PAY * 2, (uint8_t)iter);
    pa.send(p.data(), (uint32_t)p.size());
    bus.advance(NX_TTL + 1);
    bus.dropPercent = 0;
  }

  // Pool must still be healthy enough to deliver a normal message.
  std::vector<uint8_t> good(50, 0x77);
  TEST_ASSERT_TRUE(pa.send(good.data(), (uint32_t)good.size()));
  Message m;
  TEST_ASSERT_TRUE(pb.receive(m));
  TEST_ASSERT_EQUAL_UINT32(50u, m.len());
}

void run_gc_timeout_tests() {
  RUN_TEST(gc_reclaims_timed_out_slot);
  RUN_TEST(gc_does_not_leak_pool_entries);
}
