#pragma once
#include "../include/octolite.hpp"

namespace octolite::CacheOps {

inline void* try_pop(LocalCache& c) noexcept {
    if (c.count > 0) return c.items[--c.count];
    return nullptr;
}

inline void fill(LocalCache& c, Bank& b) noexcept {
    uint64_t head = b.free_head.load(std::memory_order_relaxed);
    for (uint32_t i = 0; i < BATCH_SIZE && head; ++i) {
        auto* node = reinterpret_cast<FreeNode*>(head);
        head = node->next.load(std::memory_order_relaxed);
        c.items[c.count++] = node;
    }
    b.free_head.store(head, std::memory_order_release);
}

inline void flush(LocalCache& c, Bank& b) noexcept {
    if (c.count == 0) return;
    uint64_t old_head = b.free_head.load(std::memory_order_relaxed);
    uint64_t new_tail = reinterpret_cast<uint64_t>(c.items[0]);

    for (uint32_t i = 1; i < c.count; ++i) {
        reinterpret_cast<FreeNode*>(c.items[i])->next.store(new_tail, std::memory_order_relaxed);
        new_tail = reinterpret_cast<uint64_t>(c.items[i]);
    }
    reinterpret_cast<FreeNode*>(c.items[c.count-1])->next.store(old_head, std::memory_order_relaxed);

    b.free_head.compare_exchange_strong(old_head, new_tail, std::memory_order_acq_rel);
    c.count = 0;
}

} // namespace octolite::CacheOps
