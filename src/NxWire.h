#pragma once

// Wire types, tunables, and constants shared across all protocol layers.
// Include this instead of NxProtocol.h in sub-layer headers to avoid
// circular dependencies.

#include <stdint.h>
#include <stddef.h>

// ---- Compile-time tunables -----------------------------------------------
#ifndef NX_PAY
  #define NX_PAY            240
#endif
#ifndef NX_WIN
  #define NX_WIN             16
#endif
#ifndef NX_POOL
  #define NX_POOL            32
#endif
#ifndef NX_MSG
  #define NX_MSG              4
#endif
#ifndef NX_TIMEOUT_MS
  #define NX_TIMEOUT_MS     300
#endif
#ifndef NX_RETRY
  #define NX_RETRY            5
#endif
#ifndef NX_MAX_BLOCKS
  #define NX_MAX_BLOCKS   43690
#endif
#ifndef NX_RXQ
  #define NX_RXQ              4
#endif
#ifndef NX_DUP
  #define NX_DUP             16
#endif
#ifndef NX_TTL
  #define NX_TTL           5000
#endif
#ifndef NX_MAX_PAYLOAD_BYTES
  #define NX_MAX_PAYLOAD_BYTES (10UL * 1024UL * 1024UL)
#endif

// ---- AES-256-GCM overhead ------------------------------------------------
#define NX_GCM_IV_LEN   12
#define NX_GCM_TAG_LEN  16
#define NX_ENC_PAY  (NX_PAY + NX_GCM_IV_LEN + NX_GCM_TAG_LEN)

// ---- Sanity checks -------------------------------------------------------
static_assert(NX_WIN  <= 32,
    "NX_WIN must be <= 32: the ACK bitmap is uint32_t");
static_assert(NX_POOL >= 1,
    "NX_POOL must be >= 1");
static_assert((NX_RXQ & (NX_RXQ - 1)) == 0,
    "NX_RXQ must be a power of two");
static_assert(__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__,
    "NowX wire format requires a little-endian target");
static_assert((size_t)NX_MAX_BLOCKS * (size_t)NX_PAY <= NX_MAX_PAYLOAD_BYTES,
    "NX_MAX_BLOCKS * NX_PAY exceeds NX_MAX_PAYLOAD_BYTES");

namespace nowx {

static constexpr uint32_t NX_MAGIC    = 0x4E585032UL;
static constexpr size_t   NX_MAX_BYTES = NX_MAX_PAYLOAD_BYTES;

enum : uint8_t {
    NX_ACK       = 1 << 0,
    NX_ENCRYPT   = 1 << 1,
    NX_COMPRESS  = 1 << 2,
    NX_STREAM    = 1 << 3,
    NX_PRIO      = 1 << 4,
    NX_IS_ACK    = 1 << 7
};

#pragma pack(push, 1)
struct NxHdr {
    uint32_t magic;
    uint32_t msg;
    uint32_t idx;
    uint32_t cnt;
    uint16_t pay;
    uint8_t  flags;
    uint32_t crc;
};
struct NxAck {
    uint8_t  type;
    uint32_t msg;
    uint32_t base;
    uint32_t ack;
    uint32_t t;
} __attribute__((packed));
#pragma pack(pop)

static_assert(sizeof(NxHdr) == 4+4+4+4+2+1+4, "NxHdr layout");
static_assert(sizeof(NxAck) == 1+4+4+4+4,      "NxAck layout");

} // namespace nowx
