#include "NowX.h"
#include "Log.h"

NowX *NowX::_me = nullptr;

static void _mac(
  const char *s,
  uint8_t *m
) {
  sscanf(
    s,
    "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
    &m[0],&m[1],&m[2],
    &m[3],&m[4],&m[5]
  );
}

uint8_t *Message::data() {
  return ptr;
}

uint32_t Message::len() {
  return lenv;
}

String Message::str() {
  return String((char*)ptr).substring(0, lenv);
}

bool Message::encrypted() {
  return flags & NX_ENCRYPT;
}

NowX::NowX(const char *name) {

  strncpy(
    _name,
    name,
    sizeof(_name)-1
  );

  _enc = false;
}

bool NowX::begin() {

  _me = this;

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  WiFi.setSleep(false);

  esp_wifi_set_ps(WIFI_PS_NONE);

  if (esp_now_init() != ESP_OK) {
    return false;
  }

  esp_now_register_recv_cb(_rxCb);
  esp_now_register_send_cb(_txCb);

  return true;
}

bool NowX::beginServer() {
  return begin();
}

void NowX::setPeer(const char *mac) {

  _mac(mac, _peer);

  esp_now_peer_info_t p = {};

  memcpy(p.peer_addr, _peer, 6);

  p.encrypt = false;

  esp_now_add_peer(&p);
}

void NowX::setKey(
  const uint8_t *key,
  uint32_t len
) {

  memset(_key, 0, sizeof(_key));

  memcpy(
    _key,
    key,
    min((uint32_t)32, len)
  );

  _enc = true;
}

uint32_t NowX::_crc(
  const uint8_t *d,
  uint32_t len
) {

  uint32_t c = 0xFFFFFFFF;

  for (uint32_t i=0; i<len; i++) {

    c ^= d[i];

    for (uint8_t j=0; j<8; j++) {

      c =
        (c >> 1) ^
        (0xEDB88320 &
        (-(int)(c & 1)));
    }
  }

  return ~c;
}

void NowX::_xor(
  uint8_t *d,
  uint32_t len
) {

  if (!_enc) return;

  for (uint32_t i=0; i<len; i++) {
    d[i] ^= _key[i & 31];
  }
}

NxSeg *NowX::_alloc() {

  for (auto &s : _pool) {

    if (!s.used) {

      s.used = true;

      return &s;
    }
  }

  return nullptr;
}

void NowX::_free(NxSeg *s) {

  if (!s) return;

  s->used = false;
}

NxAsm *NowX::_asmGet(uint32_t msg) {

  for (auto &a : _asm) {
    if (a.used && a.msg == msg) {
      return &a;
    }
  }

  for (auto &a : _asm) {

    if (!a.used) {

      memset(&a, 0, sizeof(a));

      a.used = true;
      a.msg = msg;

      return &a;
    }
  }

  return nullptr;
}

bool NowX::_sendRaw(
  const uint8_t *d,
  uint32_t len
) {
  return esp_now_send(
    _peer,
    (uint8_t*)d,
    len
  ) == ESP_OK;
}

bool NowX::send(
  const String &s,
  uint8_t flags
) {
  return send(
    (uint8_t*)s.c_str(),
    s.length(),
    flags
  );
}

bool NowX::send(
  const uint8_t *d,
  uint32_t len,
  uint8_t flags
) {

  uint32_t msg = ++_msg;

  uint16_t total =
    (len + NX_PAY - 1) / NX_PAY;

  uint8_t pkt[256];

  for (uint16_t base=0;
       base<total;
       base += NX_WIN) {

    uint16_t end =
      min(
        (uint16_t)(base + NX_WIN),
        total
      );

    for (uint16_t b=base; b<end; b++) {

      uint32_t off = b * NX_PAY;

      uint16_t sz =
        min(
          (uint32_t)NX_PAY,
          len - off
        );

      NxHdr h = {};

      h.magic = NX_MAGIC;
      h.msg = msg;
      h.blk = b;
      h.total = total;
      h.len = sz;
      h.flags = flags;
      h.crc = _crc(d + off, sz);

      memcpy(pkt, &h, sizeof(h));

      memcpy(
        pkt + sizeof(h),
        d + off,
        sz
      );

      if (flags & NX_ENCRYPT) {
        _xor(
          pkt + sizeof(h),
          sz
        );
      }

      if (!_sendRaw(
        pkt,
        sizeof(h) + sz
      )) {
        return false;
      }
    }

    bool ok = false;

    for (int r=0; r<NX_RETRY; r++) {

      if (_waitAck(msg, base)) {
        ok = true;
        break;
      }

      logf(
        "retry win=%u",
        base
      );
    }

    if (!ok) {
      return false;
    }
  }

  return true;
}

bool NowX::_waitAck(
  uint32_t msg,
  uint32_t base
) {

  uint32_t st = millis();

  while (
    millis() - st < NX_TIMEOUT
  ) {

    if (
      _ack &&
      _lastAck.msg == msg &&
      _lastAck.base == base
    ) {

      _ack = false;

      return true;
    }

    delay(1);
  }

  return false;
}

void NowX::_sendAck(
  uint32_t msg,
  uint32_t base,
  uint32_t map,
  const uint8_t *mac
) {

  NxAck a = {};

  a.msg = msg;
  a.base = base;
  a.map = map;
  a.ts = millis();

  esp_now_send(
    mac,
    (uint8_t*)&a,
    sizeof(a)
  );
}

void NowX::_rxCb(
  const esp_now_recv_info_t *i,
  const uint8_t *d,
  int len
) {

  if (_me) {
    _me->_rxPkt(
      i->src_addr,
      d,
      len
    );
  }
}

void NowX::_txCb(
  const uint8_t *mac,
  esp_now_send_status_t s
) {
}

void NowX::_rxPkt(
  const uint8_t *mac,
  const uint8_t *d,
  int len
) {

  if (len == sizeof(NxAck)) {

    memcpy(
      (void*)&_lastAck,
      d,
      sizeof(NxAck)
    );

    _ack = true;

    return;
  }

  if (len < (int)sizeof(NxHdr)) {
    return;
  }

  NxHdr h;

  memcpy(&h, d, sizeof(h));

  if (h.magic != NX_MAGIC) {
    return;
  }

  const uint8_t *pay =
    d + sizeof(h);

  if (_crc(pay, h.len) != h.crc) {
    return;
  }

  NxAsm *a = _asmGet(h.msg);

  if (!a) return;

  a->total = h.total;

  if (a->seg[h.blk]) {
    return;
  }

  NxSeg *s = _alloc();

  if (!s) return;

  memcpy(s->data, pay, h.len);

  if (h.flags & NX_ENCRYPT) {
    _xor(s->data, h.len);
  }

  s->len = h.len;

  a->seg[h.blk] = s;

  a->got++;

  a->bits[h.blk / 32] |=
    1UL << (h.blk & 31);

  uint32_t map = 0;

  for (uint8_t i=0; i<32; i++) {

    uint16_t idx = i;

    if (
      idx < h.total &&
      a->seg[idx]
    ) {
      map |= 1UL << i;
    }
  }

  _sendAck(
    h.msg,
    0,
    map,
    mac
  );

  if (a->got == a->total) {

    static uint8_t big[65535];

    uint32_t pos = 0;

    for (uint16_t i=0;
         i<a->total;
         i++) {

      NxSeg *x = a->seg[i];

      memcpy(
        big + pos,
        x->data,
        x->len
      );

      pos += x->len;

      _free(x);
    }

    Message &m =
      _rx[_rh & 7];

    m.msg = h.msg;
    m.flags = h.flags;
    m.ptr = big;
    m.lenv = pos;

    _rh++;

    memset(a, 0, sizeof(NxAsm));
  }
}

bool NowX::receive(Message &m) {

  if (_rt == _rh) {
    return false;
  }

  m = _rx[_rt & 7];

  _rt++;

  return true;
}
