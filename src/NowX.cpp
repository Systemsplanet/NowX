#include "NowX.h"

#ifndef NOWX_HOST_BUILD
#include <string.h>

namespace nowx {

  NowX::NowX(const char *name) : _p(&_t) {
    memset(_name, 0, sizeof(_name));
    if (name) strncpy(_name, name, sizeof(_name) - 1);
  }

  bool NowX::begin() { return _t.begin(); }

  void NowX::setPeer(const char *mac) {
    if (!_p.setPeerStr(mac)) return;
    uint8_t m[6];
    NxProtocol::parseMac(mac, m);
    _t.addPeer(m);
  }

  void NowX::setKey(const uint8_t *key, uint32_t len) {
    _p.setKey(key, len);
  }

  bool NowX::send(const uint8_t *d, uint32_t len, uint8_t flags) {
    return _p.send(d, len, flags);
  }

  bool NowX::send(const String &s, uint8_t flags) {
    return _p.send(reinterpret_cast<const uint8_t*>(s.c_str()),
                   (uint32_t)s.length(), flags);
  }

  bool NowX::receive(Message &m) { return _p.receive(m); }

} // namespace nowx
#endif // !NOWX_HOST_BUILD
