#pragma once

// NxProtocol - pure protocol logic; no Arduino / ESP-IDF dependencies.
// Used by NowX (production) and by host-side Unity tests via NxLoopback.

#include "ITransport.h"
#include <stdint.h>
#include <stddef.h>
#include <string>

// ---- Compile-time tunables -------------------------------------------
// Override any of these before including NxProtocol.h.
#ifndef NX_PAY
  static_assert(true); // just guard the block
  #define NX_PAY       240   // payload bytes per fragment
#endif
#ifndef NX_WIN
  #define NX_WIN        16   // fragments per ACK window (max 32)
#endif
#ifndef NX_POOL
  #define NX_POOL      128   // total fragment buffer slots (>= NX_MSG * NX_WIN)
#endif
#ifndef NX_MSG
  #define NX_MSG         8   // concurrent reassembly slots
#endif
#ifndef NX_TIMEOUT_MS
  #define NX_TIMEOUT_MS 300  // per-window ACK wait (ms)
#endif
#ifndef NX_RETRY
  #define NX_RETRY       5   // retransmit attempts per window
#endif
#ifndef NX_MAX_BLOCKS
  #define NX_MAX_BLOCKS 256  // max fragments per message (~60 KB)
#endif
#ifndef NX_RXQ
  #define NX_RXQ         8   // receive queue depth (power of two)
#endif
#ifndef NX_DUP
  #define NX_DUP        16   // per-peer dedup history depth
#endif
#ifndef NX_TTL
  #define NX_TTL      5000   // stale reassembly slot timeout (ms)
#endif

// ---- Sanity checks ---------------------------------------------------
static_assert(NX_WIN  <= 32,
  "NX_WIN must be <= 32: the ACK bitmap is uint32_t");
static_assert(NX_POOL >= NX_MSG * NX_WIN,
  "NX_POOL must be >= NX_MSG * NX_WIN to avoid starvation");
static_assert((NX_RXQ & (NX_RXQ - 1)) == 0,
  "NX_RXQ must be a power of two");
static_assert(__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__,
  "NowX wire format requires a little-endian target");

namespace nowx {

  // ---- Constants -------------------------------------------------------
  static constexpr uint32_t NX_MAGIC   = 0x4E585031UL; // 'NXP1'
  static constexpr size_t   NX_MAX_BYTES =
      (size_t)NX_MAX_BLOCKS * (size_t)NX_PAY;

  // ---- Packet flags ----------------------------------------------------
  enum : uint8_t {
    NX_ACK       = 1 << 0,
    NX_OBFUSCATE = 1 << 1,
    NX_COMPRESS  = 1 << 2, // reserved
    NX_STREAM    = 1 << 3, // reserved
    NX_PRIO      = 1 << 4, // reserved
    NX_IS_ACK    = 1 << 7  // set in standalone ACK packets (no NxHdr magic)
  };

  // ---- Wire structs ----------------------------------------------------
  // All multi-byte fields are little-endian (ESP32 native).
  #pragma pack(push, 1)
  struct NxHdr {
    uint32_t magic; // NX_MAGIC
    uint32_t msg;   // monotonically increasing message id
    uint16_t idx;   // fragment index within this message
    uint16_t cnt;   // total fragment count
    uint16_t pay;   // payload bytes in this frame (<= NX_PAY)
    uint8_t  flags; // NX_OBFUSCATE | ...
    uint32_t crc;   // CRC32 of on-wire payload bytes
  };
  struct NxAck {
    uint8_t  type;  // NX_IS_ACK — distinguishes ACK frames from data
    uint32_t msg;   // message being acked
    uint32_t base;  // window base fragment index
    uint32_t ack;   // bitmap of received fragments within this window
    uint32_t t;     // sender timestamp (informational, ms)
  } __attribute__((packed));
  #pragma pack(pop)

  static_assert(sizeof(NxHdr) == 4+4+2+2+2+1+4, "NxHdr layout");
  static_assert(sizeof(NxAck) == 1+4+4+4+4,      "NxAck layout");

  // ---- Public message handle ------------------------------------------
  class NxProtocol;

  class Message {
  public:
    Message() = default;

    // Automatically releases the buffer on destruction.
    ~Message() { release(); }

    // Non-copyable; move is fine.
    Message(const Message&)            = delete;
    Message& operator=(const Message&) = delete;
    Message(Message&& o) noexcept;
    Message& operator=(Message&& o) noexcept;

    uint8_t  *data()       const { return _ptr; }
    uint32_t  len()        const { return _len; }
    uint32_t  id()         const { return _msg; }
    bool      obfuscated() const { return (_flags & NX_OBFUSCATE) != 0; }

    // Safe for payloads with embedded NUL bytes.
    std::string str() const;

    // Return the reassembly buffer to the pool. Idempotent.
    // Normally called automatically by the destructor.
    void release();

    // Internal — called by NxProtocol::receive().
    void _bind(NxProtocol *p, uint8_t *ptr, uint32_t len,
               uint8_t flags, uint32_t msg, int slot);

  private:
    NxProtocol *_owner = nullptr;
    int         _slot  = -1;
    uint8_t    *_ptr   = nullptr;
    uint32_t    _len   = 0;
    uint8_t     _flags = 0;
    uint32_t    _msg   = 0;
  };

  // ---- Protocol engine ------------------------------------------------
  class NxProtocol {
  public:
    explicit NxProtocol(ITransport *t);

    // Configure the (single) peer this protocol talks to.
    void setPeer(const uint8_t mac[6]);
    bool setPeerStr(const char *s); // "AA:BB:CC:DD:EE:FF"; false on error

    // Set an XOR obfuscation key. Pass nullptr/0 to disable.
    // NOTE: XOR is NOT cryptography — see README.
    void setKey(const uint8_t *key, uint32_t len);

    // Send bytes. Returns true when all windows are ACKed.
    // May block for up to NX_TIMEOUT_MS * NX_RETRY * numWindows ms.
    bool send(const uint8_t *data, uint32_t len, uint8_t flags = 0);
    bool send(const std::string &s,              uint8_t flags = 0);

    // Pop one fully-reassembled message, if available.
    bool receive(Message &out);

    // ---- Pure helpers exposed for testing ----------------------------
    static uint32_t crc32(const uint8_t *d, uint32_t len);
    static bool     parseMac(const char *s, uint8_t mac[6]);
    void            xorInPlace(uint8_t *d, uint32_t len) const;

    // ---- Called by Message -------------------------------------------
    void _releaseSlot(int slot);

    // ---- Called by transport (public for the binding shim) -----------
    void _onPacket(const uint8_t mac[6], const uint8_t *d, size_t len);

    // ---- Test hooks --------------------------------------------------
    bool hasObfuscation() const { return _obf; }

  private:
    // ----- TX state ---------------------------------------------------
    struct TxState {
      uint8_t  peer[6]{};
      bool     peerSet = false;
      uint32_t msgCtr  = 0;
    } _tx;

    // ----- RX reassembly state ----------------------------------------
    // Frag: one fragment-sized buffer from the pool.
    struct Frag {
      bool    used;
      uint16_t len;
      uint8_t  data[NX_PAY];
    };
    // RxAsm: one in-flight reassembly slot.
    struct RxAsm {
      bool     used;
      uint32_t msg;
      uint8_t  srcMac[6];
      uint16_t cnt;    // expected fragment count
      uint16_t got;    // received so far
      uint32_t lastMs;
      Frag    *frag[NX_MAX_BLOCKS];
      // Note: frag[i] != nullptr is the authoritative "received" test;
      //       no separate bitmap needed.
    };
    // RxMsg: a fully-reassembled message waiting for the caller.
    struct RxMsg {
      bool     used;
      uint8_t  buf[NX_MAX_BYTES];
      uint32_t len;
      uint8_t  flags;
      uint32_t msg;
    };
    struct DedupEntry {
      uint8_t  mac[6];
      uint32_t msg;
    };

    // ----- ACK signalling (shared between TX task and RX callback) ----
    // Guarded by _ackLock on-device; plain variables on host.
#ifdef NOWX_HOST_BUILD
    bool   _ackReady = false;
    NxAck  _lastAck{};
#else
    volatile bool _ackReady = false;
    NxAck         _lastAck{};
    portMUX_TYPE  _ackLock = portMUX_INITIALIZER_UNLOCKED;
#endif

    // ----- Obfuscation ------------------------------------------------
    uint8_t _key[32]{};
    bool    _obf = false;

    // ----- Pools & queues ---------------------------------------------
    ITransport *_t;

    Frag  _pool[NX_POOL]{};
    RxAsm _asm[NX_MSG]{};

    RxMsg            _rxSlots[NX_RXQ]{};
    uint8_t          _rxQueue[NX_RXQ]{};
    volatile uint8_t _rxH = 0; // producer writes here
    volatile uint8_t _rxT = 0; // consumer reads here

    DedupEntry _dedup[NX_DUP]{};
    uint8_t    _dedupHead = 0;

    // ----- Internals --------------------------------------------------
    Frag  *_allocFrag();
    void   _freeFrag(Frag *f);
    RxAsm *_asmGet(uint32_t msg, const uint8_t mac[6]);
    void   _asmGc();
    int    _rxAlloc();
    bool   _isDup(const uint8_t mac[6], uint32_t msg);
    void   _markSeen(const uint8_t mac[6], uint32_t msg);

    bool _sendRaw(const uint8_t *d, uint32_t len);
    bool _waitAck(uint32_t msg, uint32_t base, uint32_t expected);
    void _sendAck(uint32_t msg, uint32_t base, uint32_t ackMap,
                  const uint8_t mac[6]);

    void _handleAck(const uint8_t *d, size_t len);
    void _handleData(const uint8_t mac[6], const uint8_t *d, size_t len);
  };
}
