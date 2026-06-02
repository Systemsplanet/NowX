#pragma once

#include "NxWire.h"
#include <stdint.h>

namespace nowx {

class NxCrypto {
public:
    NxCrypto() = default;

    void setKey(const uint8_t *key, uint32_t len);
    bool hasKey() const { return _enabled; }

    bool encryptFragment(const uint8_t *plain, uint16_t ptLen,
                         uint8_t *dst, uint16_t &outLen) const;

    bool decryptFragment(const uint8_t *enc, uint16_t encLen,
                         uint8_t *dst, uint16_t &outLen) const;

private:
    uint8_t _key[32]{};
    bool    _enabled = false;
};

} // namespace nowx
