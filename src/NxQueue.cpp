#include "NxQueue.h"

namespace nowx {

NxQueue::NxQueue(int depth)
    : _depth(depth), _head(0), _tail(0)
{
    _slots = new RxMsg[depth]();
    _ring  = new uint8_t[depth]();
}

NxQueue::~NxQueue() {
    for (int i = 0; i < _depth; i++) {
        if (_slots[i].buf) { free(_slots[i].buf); _slots[i].buf = nullptr; }
    }
    delete[] _slots;
    delete[] _ring;
}

int NxQueue::alloc() {
    for (int i = 0; i < _depth; i++) {
        if (!_slots[i].used) { _slots[i].used = true; return i; }
    }
    return -1;
}

void NxQueue::release(int slot) {
    if (slot < 0 || slot >= _depth) return;
    RxMsg &r = _slots[slot];
    if (r.buf) { free(r.buf); r.buf = nullptr; }
    r.len  = 0;
    r.used = false;
}

bool NxQueue::enqueue(int slot) {
    uint8_t next = (uint8_t)((_head + 1) & (_depth - 1));
    if (next == _tail) return false;
    _ring[_head] = (uint8_t)slot;
    _head = next;
    return true;
}

bool NxQueue::dequeue(int &slotOut) {
    if (_tail == _head) return false;
    slotOut = _ring[_tail];
    _tail = (uint8_t)((_tail + 1) & (_depth - 1));
    return true;
}

bool NxQueue::empty() const { return _tail == _head; }

} // namespace nowx
