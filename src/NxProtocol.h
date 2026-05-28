#pragma once

// NxProtocol - pure protocol logic, no Arduino / ESP-IDF dependencies.
// Used by NowX (production) and by host-side Unity tests via LoopbackTransport.

#include "ITransport.h"
#include <stdint.h>
#include <stddef.h>
#include <string>

namespace nowx {

  // ---- Tunables ---------------------------------------------------------
  static constexpr uint32_t NX_MAGIC = 0x4E585031UL; // 'NXP1'
  static constexpr uint16_t NX_PAY = 240;
  static constexpr uint16_t NX_WIN = 16;
  static constexpr uint16_t NX_POOL = 64;
  static constexpr uint8_t NX_MSG = 8;
  static constexpr uint32_t NX_TIMEOUT_MS = 300;
  static constexpr uint8_t NX_RETRY = 5;
  static constexpr uint16_t NX_MAX_BLOCKS = 256;
  static constexpr uint8_t NX_RX_QUEUE = 8; // power of two
  static constexpr uint8_t NX_DEDUP = 16;
  static constexpr uint32_t NX_ASM_TTL_MS = 5000;

  static constexpr size_t NX_MAX_BYTES =
      (size_t)NX_MAX_BLOCKS * (size_t)NX_PAY;

  enum : uint8_t {
    NX_ACK       = 1 << 0,
    NX_OBFUSCATE = 1 << 1,
    NX_COMPRESS  = 1 << 2, // reserved
    NX_STREAM    = 1 << 3, // reserved
    NX_PRIO      = 1 << 4  // reserved
  };

  #pragma pack(push, 1)
  struct NxHdr {
    uint32_t magic;
    uint32_t msg;
    uint16_t blk;
    uint16_t total;
    uint16_t len;
    uint8_t  flags;
    uint32_t crc; // CRC32 of on-wire payload bytes
  };
  struct NxAck {
    uint32_t msg;
    uint32_t base;
    uint32_t map;
    uint32_t ts;
  };
  #pragma pack(pop)

  static_assert(sizeof(NxHdr) == 4+4+2+2+2+1+4, "NxHdr layout");
  static_assert(sizeof(NxAck) == 16, "NxAck layout");

  // ---- Public message handle -------------------------------------------
  class NxProtocol;

  class Message {
  public:
    Message() = default;

    uint8_t  *data() const { return _ptr; }
    uint32_t  len()  const { return _len; }
    bool obfuscated() const { return (_flags & NX_OBFUSCATE) != 0; }
    uint32_t  id()   const { return _msg; }

    // Safe even for non-NUL-terminated payloads.
    std::string str() const;

    // Hand the reassembly buffer back to the pool. Idempotent.
    void release();

    // Used internally by NxProtocol::receive().
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

  // ---- Protocol engine --------------------------------------------------
  class NxProtocol {
  public:
    explicit NxProtocol(ITransport *t);

    // Configure the (single) peer this protocol talks to.
    void setPeer(const uint8_t mac[6]);
    // "AA:BB:CC:DD:EE:FF" - returns false on parse error.
    bool setPeerStr(const char *s);

    // Set an XOR obfuscation key. Pass nullptr/0 to disable.
    // NOTE: This is *not* cryptography. See README.
    void setKey(const uint8_t *key, uint32_t len);

    // Send raw bytes; returns true on full delivery (all windows ACKed).
    bool send(const uint8_t *data, uint32_t len, uint8_t flags = 0);
    bool send(const std::string &s, uint8_t flags = 0);

    // Pop one fully-reassembled message, if available.
    bool receive(Message &out);

    // ---- Pure functions exposed for testing ---------------------------
    static uint32_t crc32(const uint8_t *d, uint32_t len);
    static bool parseMac(const char *s, uint8_t mac[6]);
    void xorInPlace(uint8_t *d, uint32_t len) const;

    // ---- Called by Message::release() ---------------------------------
    void _releaseSlot(int slot);

    // ---- Called by transport (public so the binding shim can reach) ---
    void _onPacket(const uint8_t mac[6], const uint8_t *d, size_t len);

    // ---- Test hooks ---------------------------------------------------
    bool hasObfuscation() const { return _enc; }

  private:
    struct Seg {
      bool    used;
      uint16_t len;
      uint8_t  data[NX_PAY];
    };
    struct Asm {
      bool     used;
      uint32_t msg;
      uint8_t  srcMac[6];
      uint16_t total;
      uint16_t got;
      uint32_t lastMs;
      Seg     *seg[NX_MAX_BLOCKS];
      uint32_t winMap[(NX_MAX_BLOCKS + 31) / 32];
    };
    struct RxSlot {
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

    ITransport *_t;
    uint8_t     _peer[6]{};
    bool        _peerSet = false;

    uint8_t _key[32]{};
    bool    _enc = false;

    uint32_t _msgCtr = 0;

    Seg     _pool[NX_POOL]{};
    Asm     _asm[NX_MSG]{};

    volatile bool _ackPending = false;
    NxAck         _lastAck{};

    RxSlot           _rxSlots[NX_RX_QUEUE]{};
    uint8_t          _rxQueue[NX_RX_QUEUE]{};
    volatile uint8_t _rh = 0;
    volatile uint8_t _rt = 0;

    DedupEntry _dedup[NX_DEDUP]{};
    uint8_t    _dedupHead = 0;

    // ---- internals ----
    Seg  *_alloc();
    void  _free(Seg *s);
    Asm  *_asmGet(uint32_t msg, const uint8_t mac[6]);
    void  _asmGc();
    int   _rxAlloc();
    bool  _isDuplicate(const uint8_t mac[6], uint32_t msg);
    void  _markSeen(const uint8_t mac[6], uint32_t msg);

    bool _sendRaw(const uint8_t *d, uint32_t len);
    bool _waitAck(uint32_t msg, uint32_t base, uint32_t expectedMap);
    void _sendAck(uint32_t msg, uint32_t base, uint32_t map,
                  const uint8_t mac[6]);

    void _handleAck(const uint8_t *d, size_t len);
    void _handleData(const uint8_t mac[6], const uint8_t *d, size_t len);
  };
}
