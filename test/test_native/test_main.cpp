#include <unity.h>

void setUp(void) {}
void tearDown(void) {}

void run_crc_tests();
void run_xor_tests();
void run_mac_tests();
void run_fragmentation_tests();
void run_reassembly_tests();
void run_ack_window_tests();
void run_encryption_roundtrip_tests();
void run_message_str_tests();
void run_logger_tests();

int main(int, char**) {
  UNITY_BEGIN();
  run_crc_tests();
  run_xor_tests();
  run_mac_tests();
  run_fragmentation_tests();
  run_reassembly_tests();
  run_ack_window_tests();
  run_encryption_roundtrip_tests();
  run_message_str_tests();
  run_logger_tests();
  return UNITY_END();
}
