#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

#define NX_MAGIC 0x4E585031UL
#define NX_PAY 240
#define NX_WIN 16
#define NX_POOL 64
#define NX_MSG 8
#define NX_TIMEOUT 300
#define NX_RETRY 5

enum {
  NX_ACK      = 1 << 0,
  NX_ENCRYPT  = 1 << 1,
  NX_COMPRESS = 1 << 2,
  NX_STREAM   = 1 << 3,
  NX_PRIO     = 1 << 4
};

struct __attribute__((packed)) NxHdr {

  uint32_t magic;
  uint32_t msg;
  uint16_t blk;
  uint16_t total;
  uint16_t len;
  uint8_t flags;
  uint32_t crc;
};

struct __attribute__((packed)) NxAck {

  uint32_t msg;
  uint32_t base;
  uint32_t map;
  uint32_t ts;
};

struct NxSeg {

  bool used;
  uint16_t len;
  uint8_t data[NX_PAY];
};

struct NxAsm {

  bool used;
  uint32_t msg;
  uint16_t total;
  uint16_t got;
  uint32_t last;
  uint32_t bits[8];
  NxSeg *seg[256];
};

class Message {

public:

  uint32_t msg;
  uint8_t flags;
  uint32_t lenv;
  uint8_t *ptr;

  uint8_t *data();
  uint32_t len();
  String str();
  bool encrypted();
};

class NowX {

public:

  NowX(const char *name);

  bool begin();
  bool beginServer();

  void setPeer(const char *mac);

  void setKey(
    const uint8_t *key,
    uint32_t len
  );

  bool send(
    const uint8_t *d,
    uint32_t len,
    uint8_t flags = 0
  );

  bool send(
    const String &s,
    uint8_t flags = 0
  );

  bool receive(Message &m);

private:

  char _name[32];

  uint8_t _peer[6];

  uint8_t _key[32];
  bool _enc;

  uint32_t _msg;

  NxSeg _pool[NX_POOL];
  NxAsm _asm[NX_MSG];

  volatile bool _ack;
  volatile NxAck _lastAck;

  Message _rx[8];
  volatile uint8_t _rh;
  volatile uint8_t _rt;

  static NowX *_me;

  static void _rxCb(
    const esp_now_recv_info_t *i,
    const uint8_t *d,
    int len
  );

  static void _txCb(
    const uint8_t *mac,
    esp_now_send_status_t s
  );

  void _rxPkt(
    const uint8_t *mac,
    const uint8_t *d,
    int len
  );

  NxSeg *_alloc();
  void _free(NxSeg *s);

  NxAsm *_asmGet(uint32_t msg);

  bool _sendRaw(
    const uint8_t *d,
    uint32_t len
  );

  bool _waitAck(
    uint32_t msg,
    uint32_t base
  );

  void _sendAck(
    uint32_t msg,
    uint32_t base,
    uint32_t map,
    const uint8_t *mac
  );

  static uint32_t _crc(
    const uint8_t *d,
    uint32_t len
  );

  void _xor(
    uint8_t *d,
    uint32_t len
  );
};
