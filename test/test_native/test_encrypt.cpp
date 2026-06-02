#include <unity.h>
#include "NxProtocol.h"
#include "NxLoopback.h"
#include <cstring>

using namespace nowx;

// encryptFragment / decryptFragment must round-trip correctly.
static void encrypt_decrypt_roundtrip() {
  LoopbackBus bus;
  uint8_t mac[6] = {1,2,3,4,5,6};
  LoopbackEndpoint ep(&bus, mac);
  NxProtocol p(&ep);

  uint8_t key[32];
  for (int i = 0; i < 32; i++) key[i] = (uint8_t)(0x11 * i + 1);
  p.setKey(key, sizeof(key));

  uint8_t plain[64], recovered[64];
  uint8_t enc[NX_ENC_PAY];
  for (int i = 0; i < 64; i++) plain[i] = (uint8_t)i;

  uint16_t encLen = 0, ptLen = 0;
  TEST_ASSERT_TRUE(p.encryptFragment(plain, 64, enc, encLen));
  TEST_ASSERT_EQUAL_UINT16(64 + NX_GCM_IV_LEN + NX_GCM_TAG_LEN, encLen);
  TEST_ASSERT_TRUE(p.decryptFragment(enc, encLen, recovered, ptLen));
  TEST_ASSERT_EQUAL_UINT16(64, ptLen);
  TEST_ASSERT_EQUAL_INT8_ARRAY(plain, recovered, 64);
}

// Two independently-encrypted copies of the same plaintext must differ
// (random IV ensures ciphertext is never deterministic).
static void encrypt_random_iv() {
  LoopbackBus bus;
  uint8_t mac[6] = {2,2,3,4,5,6};
  LoopbackEndpoint ep(&bus, mac);
  NxProtocol p(&ep);

  uint8_t key[32] = {};
  p.setKey(key, sizeof(key));

  uint8_t plain[32] = {};
  uint8_t enc1[NX_ENC_PAY], enc2[NX_ENC_PAY];
  uint16_t len1 = 0, len2 = 0;
  TEST_ASSERT_TRUE(p.encryptFragment(plain, 32, enc1, len1));
  TEST_ASSERT_TRUE(p.encryptFragment(plain, 32, enc2, len2));
  // The IVs (first 12 bytes) must differ with overwhelming probability.
  TEST_ASSERT_NOT_EQUAL(0, std::memcmp(enc1, enc2, NX_GCM_IV_LEN));
}

// Tampered ciphertext must fail authentication.
static void decrypt_rejects_tampered_ciphertext() {
  LoopbackBus bus;
  uint8_t mac[6] = {3,2,3,4,5,6};
  LoopbackEndpoint ep(&bus, mac);
  NxProtocol p(&ep);

  uint8_t key[32];
  for (int i = 0; i < 32; i++) key[i] = (uint8_t)(i + 7);
  p.setKey(key, sizeof(key));

  uint8_t plain[48], enc[NX_ENC_PAY], out[48];
  for (int i = 0; i < 48; i++) plain[i] = (uint8_t)(i ^ 0xA5);
  uint16_t encLen = 0, ptLen = 0;
  TEST_ASSERT_TRUE(p.encryptFragment(plain, 48, enc, encLen));

  // Flip one bit in the ciphertext body (after the IV).
  enc[NX_GCM_IV_LEN] ^= 0x01;
  TEST_ASSERT_FALSE(p.decryptFragment(enc, encLen, out, ptLen));
}

// Tampered tag must fail authentication.
static void decrypt_rejects_tampered_tag() {
  LoopbackBus bus;
  uint8_t mac[6] = {4,2,3,4,5,6};
  LoopbackEndpoint ep(&bus, mac);
  NxProtocol p(&ep);

  uint8_t key[32];
  for (int i = 0; i < 32; i++) key[i] = (uint8_t)(i * 3 + 5);
  p.setKey(key, sizeof(key));

  uint8_t plain[16], enc[NX_ENC_PAY], out[16];
  for (int i = 0; i < 16; i++) plain[i] = (uint8_t)i;
  uint16_t encLen = 0, ptLen = 0;
  TEST_ASSERT_TRUE(p.encryptFragment(plain, 16, enc, encLen));

  // Flip one bit in the tag (last 16 bytes).
  enc[encLen - 1] ^= 0x80;
  TEST_ASSERT_FALSE(p.decryptFragment(enc, encLen, out, ptLen));
}

// Without a key, encryptFragment must return false.
static void encrypt_disabled_returns_false() {
  LoopbackBus bus;
  uint8_t mac[6] = {5,2,3,4,5,6};
  LoopbackEndpoint ep(&bus, mac);
  NxProtocol p(&ep); // no setKey call

  uint8_t plain[16] = {}, enc[NX_ENC_PAY];
  uint16_t outLen = 0;
  TEST_ASSERT_FALSE(p.encryptFragment(plain, 16, enc, outLen));
}

void run_encrypt_tests() {
  RUN_TEST(encrypt_decrypt_roundtrip);
  RUN_TEST(encrypt_random_iv);
  RUN_TEST(decrypt_rejects_tampered_ciphertext);
  RUN_TEST(decrypt_rejects_tampered_tag);
  RUN_TEST(encrypt_disabled_returns_false);
}
