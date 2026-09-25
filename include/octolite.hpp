#pragma once
#include "pch.hpp"

namespace octolite {

constexpr uint64_t CACHE_SIZE{64};
constexpr uint64_t GPU_CACHE_SIZE{256};
constexpr uintptr_t ADDR_MASK = 0x0000FFFFFFFFFFFFULL;
constexpr int TAG_SHIFT = 48;
constexpr size_t NUM_SLOT_GROUPS = 4;
constexpr size_t CHUNK_SIZE = 256;
constexpr size_t BANK_CAPACITY = 4096;
constexpr uint32_t LOCAL_CACHE_SIZE = 32;
constexpr uint32_t BATCH_SIZE = 32;

static_assert(CHUNK_SIZE == GPU_CACHE_SIZE, "chunk must equal GPU cache line");

struct alignas(CACHE_SIZE) FreeNode {
    std::atomic<uint64_t> next{0};
};

struct TaggedPtr {
    static constexpr void *pack(void *ptr, const uint16_t tag) noexcept {
        return reinterpret_cast<void *>(
            (reinterpret_cast<uintptr_t>(ptr) & ADDR_MASK) |
            (static_cast<uintptr_t>(tag) << TAG_SHIFT));
    }
    static constexpr void *untag(void *tagged) noexcept {
        return reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(tagged) & ADDR_MASK);
    }
    static constexpr uint16_t tag_of(void *tagged) noexcept {
           return static_cast<uint16_t>(reinterpret_cast<uintptr_t>(tagged) >> TAG_SHIFT);
       }
};

struct alignas(CACHE_SIZE) LocalCache {
    void *items[LOCAL_CACHE_SIZE]{};
    uint32_t count{0};
    LocalCache() = default;
    LocalCache(const LocalCache &) = delete("thread-confined");
    LocalCache &operator=(const LocalCache &) = delete("thread-confined");
};

struct alignas(CACHE_SIZE) Bank {
    std::atomic<uint64_t> free_head{0};
    std::atomic<uint32_t> active_count{0};
};

} // namespace octolite
