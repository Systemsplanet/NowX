#include "NxCrypto.h"
#include <string.h>

// ---- Platform crypto selection ------------------------------------------
// ESP32 device builds use mbedtls from the SDK.
// Host builds use system mbedtls when available, otherwise a portable
// software AES-256-GCM fallback (test/CI only — not constant-time).
// -------------------------------------------------------------------------

#ifndef NOWX_HOST_BUILD

  #include <mbedtls/gcm.h>
  #include <esp_random.h>

  static void nx_random_bytes(uint8_t *out, size_t len) {
      esp_fill_random(out, len);
  }

  static bool nx_aes256gcm_encrypt(const uint8_t key[32],
                                    const uint8_t *iv,
                                    const uint8_t *plain, uint16_t ptLen,
                                    uint8_t *ct, uint8_t *tag) {
      mbedtls_gcm_context gcm;
      mbedtls_gcm_init(&gcm);
      bool ok = (mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key, 256) == 0) &&
                (mbedtls_gcm_crypt_and_tag(&gcm, MBEDTLS_GCM_ENCRYPT,
                                            ptLen, iv, NX_GCM_IV_LEN,
                                            nullptr, 0,
                                            plain, ct,
                                            NX_GCM_TAG_LEN, tag) == 0);
      mbedtls_gcm_free(&gcm);
      return ok;
  }

  static bool nx_aes256gcm_decrypt(const uint8_t key[32],
                                    const uint8_t *iv,
                                    const uint8_t *ct, uint16_t ctLen,
                                    const uint8_t *tag,
                                    uint8_t *plain) {
      mbedtls_gcm_context gcm;
      mbedtls_gcm_init(&gcm);
      bool ok = (mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key, 256) == 0) &&
                (mbedtls_gcm_auth_decrypt(&gcm, ctLen,
                                          iv, NX_GCM_IV_LEN,
                                          nullptr, 0,
                                          tag, NX_GCM_TAG_LEN,
                                          ct, plain) == 0);
      mbedtls_gcm_free(&gcm);
      return ok;
  }

#else // NOWX_HOST_BUILD

  #if __has_include(<mbedtls/gcm.h>)

    #include <mbedtls/gcm.h>
    #include <stdio.h>

    static void nx_random_bytes(uint8_t *out, size_t len) {
        FILE *f = fopen("/dev/urandom", "rb");
        if (f) { (void)fread(out, 1, len, f); fclose(f); }
        else   { for (size_t i = 0; i < len; i++) out[i] = (uint8_t)(i ^ 0xA5); }
    }

    static bool nx_aes256gcm_encrypt(const uint8_t key[32],
                                      const uint8_t *iv,
                                      const uint8_t *plain, uint16_t ptLen,
                                      uint8_t *ct, uint8_t *tag) {
        mbedtls_gcm_context gcm;
        mbedtls_gcm_init(&gcm);
        bool ok = (mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key, 256) == 0) &&
                  (mbedtls_gcm_crypt_and_tag(&gcm, MBEDTLS_GCM_ENCRYPT,
                                              ptLen, iv, NX_GCM_IV_LEN,
                                              nullptr, 0,
                                              plain, ct,
                                              NX_GCM_TAG_LEN, tag) == 0);
        mbedtls_gcm_free(&gcm);
        return ok;
    }

    static bool nx_aes256gcm_decrypt(const uint8_t key[32],
                                      const uint8_t *iv,
                                      const uint8_t *ct, uint16_t ctLen,
                                      const uint8_t *tag,
                                      uint8_t *plain) {
        mbedtls_gcm_context gcm;
        mbedtls_gcm_init(&gcm);
        bool ok = (mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key, 256) == 0) &&
                  (mbedtls_gcm_auth_decrypt(&gcm, ctLen,
                                            iv, NX_GCM_IV_LEN,
                                            nullptr, 0,
                                            tag, NX_GCM_TAG_LEN,
                                            ct, plain) == 0);
        mbedtls_gcm_free(&gcm);
        return ok;
    }

  #else
    // Portable software AES-256-GCM fallback.
    // WARNING: Not constant-time. For host-side unit tests only.
    #include <stdio.h>
    #include <stdint.h>

    static void nx_random_bytes(uint8_t *out, size_t len) {
        FILE *f = fopen("/dev/urandom", "rb");
        if (f) { (void)fread(out, 1, len, f); fclose(f); }
        else   { for (size_t i = 0; i < len; i++) out[i] = (uint8_t)(i ^ 0xA5); }
    }

    static const uint8_t kSBox[256] = {
        0x63,0x7c,0x77,0x7b,0xf2,0x6b,0x6f,0xc5,0x30,0x01,0x67,0x2b,0xfe,0xd7,0xab,0x76,
        0xca,0x82,0xc9,0x7d,0xfa,0x59,0x47,0xf0,0xad,0xd4,0xa2,0xaf,0x9c,0xa4,0x72,0xc0,
        0xb7,0xfd,0x93,0x26,0x36,0x3f,0xf7,0xcc,0x34,0xa5,0xe5,0xf1,0x71,0xd8,0x31,0x15,
        0x04,0xc7,0x23,0xc3,0x18,0x96,0x05,0x9a,0x07,0x12,0x80,0xe2,0xeb,0x27,0xb2,0x75,
        0x09,0x83,0x2c,0x1a,0x1b,0x6e,0x5a,0xa0,0x52,0x3b,0xd6,0xb3,0x29,0xe3,0x2f,0x84,
        0x53,0xd1,0x00,0xed,0x20,0xfc,0xb1,0x5b,0x6a,0xcb,0xbe,0x39,0x4a,0x4c,0x58,0xcf,
        0xd0,0xef,0xaa,0xfb,0x43,0x4d,0x33,0x85,0x45,0xf9,0x02,0x7f,0x50,0x3c,0x9f,0xa8,
        0x51,0xa3,0x40,0x8f,0x92,0x9d,0x38,0xf5,0xbc,0xb6,0xda,0x21,0x10,0xff,0xf3,0xd2,
        0xcd,0x0c,0x13,0xec,0x5f,0x97,0x44,0x17,0xc4,0xa7,0x7e,0x3d,0x64,0x5d,0x19,0x73,
        0x60,0x81,0x4f,0xdc,0x22,0x2a,0x90,0x88,0x46,0xee,0xb8,0x14,0xde,0x5e,0x0b,0xdb,
        0xe0,0x32,0x3a,0x0a,0x49,0x06,0x24,0x5c,0xc2,0xd3,0xac,0x62,0x91,0x95,0xe4,0x79,
        0xe7,0xc8,0x37,0x6d,0x8d,0xd5,0x4e,0xa9,0x6c,0x56,0xf4,0xea,0x65,0x7a,0xae,0x08,
        0xba,0x78,0x25,0x2e,0x1c,0xa6,0xb4,0xc6,0xe8,0xdd,0x74,0x1f,0x4b,0xbd,0x8b,0x8a,
        0x70,0x3e,0xb5,0x66,0x48,0x03,0xf6,0x0e,0x61,0x35,0x57,0xb9,0x86,0xc1,0x1d,0x9e,
        0xe1,0xf8,0x98,0x11,0x69,0xd9,0x8e,0x94,0x9b,0x1e,0x87,0xe9,0xce,0x55,0x28,0xdf,
        0x8c,0xa1,0x89,0x0d,0xbf,0xe6,0x42,0x68,0x41,0x99,0x2d,0x0f,0xb0,0x54,0xbb,0x16
    };
    static const uint8_t kRcon[11] = {
        0x00,0x01,0x02,0x04,0x08,0x10,0x20,0x40,0x80,0x1b,0x36
    };

    static inline uint8_t nx_xtime(uint8_t x) {
        return (uint8_t)((x << 1) ^ (((x >> 7) & 1) * 0x1b));
    }

    static void nx_aes256_expand(const uint8_t key[32], uint8_t rk[240]) {
        memcpy(rk, key, 32);
        for (int i = 8; i < 60; i++) {
            uint8_t t[4];
            memcpy(t, rk + (i - 1) * 4, 4);
            if (i % 8 == 0) {
                uint8_t tmp = t[0];
                t[0] = kSBox[t[1]]; t[1] = kSBox[t[2]];
                t[2] = kSBox[t[3]]; t[3] = kSBox[tmp];
                t[0] ^= kRcon[i / 8];
            } else if (i % 8 == 4) {
                for (int j = 0; j < 4; j++) t[j] = kSBox[t[j]];
            }
            for (int j = 0; j < 4; j++)
                rk[i * 4 + j] = rk[(i - 8) * 4 + j] ^ t[j];
        }
    }

    static void nx_aes256_block(const uint8_t rk[240],
                                 const uint8_t in[16], uint8_t out[16]) {
        uint8_t s[16];
        memcpy(s, in, 16);
        for (int i = 0; i < 16; i++) s[i] ^= rk[i];

        for (int r = 1; r < 14; r++) {
            uint8_t t[16];
            t[0]=kSBox[s[0]];  t[1]=kSBox[s[5]];  t[2]=kSBox[s[10]]; t[3]=kSBox[s[15]];
            t[4]=kSBox[s[4]];  t[5]=kSBox[s[9]];  t[6]=kSBox[s[14]]; t[7]=kSBox[s[3]];
            t[8]=kSBox[s[8]];  t[9]=kSBox[s[13]]; t[10]=kSBox[s[2]]; t[11]=kSBox[s[7]];
            t[12]=kSBox[s[12]]; t[13]=kSBox[s[1]]; t[14]=kSBox[s[6]]; t[15]=kSBox[s[11]];
            for (int c = 0; c < 4; c++) {
                uint8_t a=t[c*4], b=t[c*4+1], cc=t[c*4+2], d=t[c*4+3];
                s[c*4]   = nx_xtime(a)^nx_xtime(b)^b^cc^d;
                s[c*4+1] = a^nx_xtime(b)^nx_xtime(cc)^cc^d;
                s[c*4+2] = a^b^nx_xtime(cc)^nx_xtime(d)^d;
                s[c*4+3] = nx_xtime(a)^a^b^cc^nx_xtime(d);
            }
            for (int i = 0; i < 16; i++) s[i] ^= rk[r * 16 + i];
        }
        uint8_t t[16];
        t[0]=kSBox[s[0]];  t[1]=kSBox[s[5]];  t[2]=kSBox[s[10]]; t[3]=kSBox[s[15]];
        t[4]=kSBox[s[4]];  t[5]=kSBox[s[9]];  t[6]=kSBox[s[14]]; t[7]=kSBox[s[3]];
        t[8]=kSBox[s[8]];  t[9]=kSBox[s[13]]; t[10]=kSBox[s[2]]; t[11]=kSBox[s[7]];
        t[12]=kSBox[s[12]]; t[13]=kSBox[s[1]]; t[14]=kSBox[s[6]]; t[15]=kSBox[s[11]];
        for (int i = 0; i < 16; i++) t[i] ^= rk[14 * 16 + i];
        memcpy(out, t, 16);
    }

    static inline uint64_t nx_be64(const uint8_t *p) {
        uint64_t v = 0;
        for (int i = 0; i < 8; i++) v = (v << 8) | p[i];
        return v;
    }
    static inline void nx_put_be64(uint8_t *p, uint64_t v) {
        for (int i = 7; i >= 0; i--) { p[i] = (uint8_t)v; v >>= 8; }
    }

    static void nx_gf128_mul(uint64_t X[2], const uint64_t H[2]) {
        uint64_t Z[2] = {0, 0}, V[2] = {H[0], H[1]};
        for (int i = 0; i < 128; i++) {
            int word = i >> 6, bit = 63 - (i & 63);
            if ((X[word] >> bit) & 1) { Z[0] ^= V[0]; Z[1] ^= V[1]; }
            int lsb = (int)(V[1] & 1);
            V[1] = (V[1] >> 1) | (V[0] << 63);
            V[0] >>= 1;
            if (lsb) V[0] ^= 0xE100000000000000ULL;
        }
        X[0] = Z[0]; X[1] = Z[1];
    }

    static void nx_ghash_ct(const uint64_t H[2],
                             const uint8_t *ct, size_t ctLen,
                             uint8_t out[16]) {
        uint64_t Y[2] = {0, 0};
        for (size_t off = 0; off < ctLen; off += 16) {
            uint8_t blk[16] = {};
            size_t n = (ctLen - off < 16) ? ctLen - off : 16;
            memcpy(blk, ct + off, n);
            Y[0] ^= nx_be64(blk);
            Y[1] ^= nx_be64(blk + 8);
            nx_gf128_mul(Y, H);
        }
        uint8_t lens[16] = {};
        nx_put_be64(lens + 8, (uint64_t)ctLen * 8);
        Y[0] ^= nx_be64(lens);
        Y[1] ^= nx_be64(lens + 8);
        nx_gf128_mul(Y, H);
        nx_put_be64(out,     Y[0]);
        nx_put_be64(out + 8, Y[1]);
    }

    static void nx_gctr(const uint8_t rk[240], const uint8_t iv[12],
                         const uint8_t *in, size_t len, uint8_t *out) {
        uint8_t ctr[16] = {};
        memcpy(ctr, iv, 12);
        ctr[15] = 2;
        for (size_t off = 0; off < len; off += 16) {
            uint8_t ks[16];
            nx_aes256_block(rk, ctr, ks);
            size_t n = (len - off < 16) ? len - off : 16;
            for (size_t j = 0; j < n; j++) out[off + j] = in[off + j] ^ ks[j];
            for (int k = 15; k >= 12; k--) if (++ctr[k]) break;
        }
    }

    static bool nx_aes256gcm_encrypt(const uint8_t key[32],
                                      const uint8_t *iv,
                                      const uint8_t *plain, uint16_t ptLen,
                                      uint8_t *ct, uint8_t *tag) {
        uint8_t rk[240];
        nx_aes256_expand(key, rk);

        uint8_t H_blk[16] = {}, EJ0[16], J0[16] = {};
        nx_aes256_block(rk, H_blk, H_blk);
        memcpy(J0, iv, 12); J0[15] = 1;
        nx_aes256_block(rk, J0, EJ0);

        nx_gctr(rk, iv, plain, ptLen, ct);

        uint8_t ghash_out[16];
        uint64_t H[2] = { nx_be64(H_blk), nx_be64(H_blk + 8) };
        nx_ghash_ct(H, ct, ptLen, ghash_out);
        for (int i = 0; i < 16; i++) tag[i] = ghash_out[i] ^ EJ0[i];
        return true;
    }

    static bool nx_aes256gcm_decrypt(const uint8_t key[32],
                                      const uint8_t *iv,
                                      const uint8_t *ct, uint16_t ctLen,
                                      const uint8_t *tag,
                                      uint8_t *plain) {
        uint8_t rk[240];
        nx_aes256_expand(key, rk);

        uint8_t H_blk[16] = {}, EJ0[16], J0[16] = {};
        nx_aes256_block(rk, H_blk, H_blk);
        memcpy(J0, iv, 12); J0[15] = 1;
        nx_aes256_block(rk, J0, EJ0);

        uint8_t ghash_out[16];
        uint64_t H[2] = { nx_be64(H_blk), nx_be64(H_blk + 8) };
        nx_ghash_ct(H, ct, ctLen, ghash_out);
        uint8_t expected_tag[16];
        for (int i = 0; i < 16; i++) expected_tag[i] = ghash_out[i] ^ EJ0[i];

        uint8_t diff = 0;
        for (int i = 0; i < 16; i++) diff |= (expected_tag[i] ^ tag[i]);
        if (diff) return false;

        nx_gctr(rk, iv, ct, ctLen, plain);
        return true;
    }

  #endif // __has_include(<mbedtls/gcm.h>)
#endif // NOWX_HOST_BUILD

// =========================================================================
// NxCrypto
// =========================================================================

namespace nowx {

void NxCrypto::setKey(const uint8_t *key, uint32_t len) {
    memset(_key, 0, sizeof(_key));
    if (!key || len == 0) { _enabled = false; return; }
    uint32_t n = (len < 32) ? len : 32;
    memcpy(_key, key, n);
    _enabled = true;
}

bool NxCrypto::encryptFragment(const uint8_t *plain, uint16_t ptLen,
                                uint8_t *dst, uint16_t &outLen) const
{
    if (!_enabled) return false;
    if ((uint32_t)ptLen + NX_GCM_IV_LEN + NX_GCM_TAG_LEN > NX_ENC_PAY) return false;

    uint8_t *iv  = dst;
    uint8_t *ct  = dst + NX_GCM_IV_LEN;
    uint8_t *tag = ct  + ptLen;

    nx_random_bytes(iv, NX_GCM_IV_LEN);
    if (!nx_aes256gcm_encrypt(_key, iv, plain, ptLen, ct, tag)) return false;

    outLen = (uint16_t)(NX_GCM_IV_LEN + ptLen + NX_GCM_TAG_LEN);
    return true;
}

bool NxCrypto::decryptFragment(const uint8_t *enc, uint16_t encLen,
                                uint8_t *dst, uint16_t &outLen) const
{
    if (!_enabled) return false;
    if (encLen < (uint16_t)(NX_GCM_IV_LEN + NX_GCM_TAG_LEN)) return false;

    uint16_t ctLen  = encLen - NX_GCM_IV_LEN - NX_GCM_TAG_LEN;
    const uint8_t *iv  = enc;
    const uint8_t *ct  = enc + NX_GCM_IV_LEN;
    const uint8_t *tag = ct  + ctLen;

    if (!nx_aes256gcm_decrypt(_key, iv, ct, ctLen, tag, dst)) return false;

    outLen = ctLen;
    return true;
}

} // namespace nowx
