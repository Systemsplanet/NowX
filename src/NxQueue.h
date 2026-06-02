#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

namespace nowx {

struct RxMsg {
    bool     used  = false;
    uint8_t *buf   = nullptr;
    uint32_t len   = 0;
    uint8_t  flags = 0;
    uint32_t msg   = 0;
};

class NxQueue {
public:
    explicit NxQueue(int depth);
    ~NxQueue();

    NxQueue(const NxQueue &)            = delete;
    NxQueue &operator=(const NxQueue &) = delete;

    int  alloc();
    void release(int slot);
    bool enqueue(int slot);
    bool dequeue(int &slotOut);
    bool empty() const;

    RxMsg &operator[](int i) { return _slots[i]; }

private:
    int     _depth;
    RxMsg  *_slots;
    uint8_t *_ring;
    volatile uint8_t _head;
    volatile uint8_t _tail;
};

} // namespace nowx
