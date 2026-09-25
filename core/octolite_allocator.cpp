#include "../include/pch.hpp"
#include "../include/octolite_allocator.hpp"
#include "octolite_platform.hpp"
#include "octolite_trace.hpp"
#include "octolite_free_list_internal.hpp"
#include <cstdio>

namespace octolite {

FreeNode* quad_pong_allocator::node_at(uint8_t group, uint8_t bank, size_t idx) noexcept {
    size_t offset = ((group * 2 + bank) * BANK_CAPACITY + idx) * CHUNK_SIZE;
    return reinterpret_cast<FreeNode*>(static_cast<char*>(pool_base_) + offset);
}

void quad_pong_allocator::init_free_list(uint8_t group, uint8_t bank) noexcept {
    Bank& b = banks_[group][bank];
    for (size_t i = 0; i < BANK_CAPACITY - 1; ++i) {
        FreeNode* cur = node_at(group, bank, i);
        FreeNode* nxt = node_at(group, bank, i + 1);
        cur->next.store(reinterpret_cast<uint64_t>(nxt), std::memory_order_relaxed);
    }
    node_at(group, bank, BANK_CAPACITY - 1)->next.store(0, std::memory_order_relaxed);
    b.free_head.store(
        reinterpret_cast<uint64_t>(node_at(group, bank, 0)),
        std::memory_order_release);
    b.active_count.store(0, std::memory_order_relaxed);
}

std::expected<quad_pong_allocator*, alloc_error>
quad_pong_allocator::create(size_t pool_size) {
    bool has_privilege = enable_lock_memory_privilege();
    void* raw = VirtualAlloc(nullptr, pool_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!raw)
        return std::unexpected(alloc_error::virtual_alloc_failed);

    std::memset(raw, 0, pool_size);
    if (has_privilege) {
        if (!VirtualLock(raw, pool_size))
            std::printf("[WARN] VirtualLock failed: %lu, continuing without lock\n", GetLastError());
    } else {
        std::printf("[WARN] lock memory privilege unavailable, continuing without VirtualLock\n");
    }

    auto* alloc = new(raw) quad_pong_allocator{};
    uintptr_t raw_addr = reinterpret_cast<uintptr_t>(raw);
    uintptr_t data_addr = (raw_addr + sizeof(quad_pong_allocator) + CHUNK_SIZE - 1) & ~(CHUNK_SIZE - 1);

    alloc->pool_raw_ = raw;
    alloc->alloc_total_ = pool_size;
    alloc->pool_base_ = reinterpret_cast<char*>(data_addr);
    alloc->pool_size_ = pool_size - static_cast<size_t>(data_addr - raw_addr);
    alloc->owns_memory_ = true;

    for (uint8_t g = 0; g < NUM_SLOT_GROUPS; ++g) {
        for (uint8_t b = 0; b < 2; ++b)
            alloc->init_free_list(g, b);
        alloc->write_bank_[g].store(0, std::memory_order_relaxed);
        alloc->caches_[g].count = 0;
    }
    return alloc;
}

std::expected<quad_pong_allocator*, alloc_error>
quad_pong_allocator::create_mapped(void* data, size_t data_size) {
    if (!data)
        return std::unexpected(alloc_error::virtual_alloc_failed);
    auto* alloc = new quad_pong_allocator{};
    alloc->pool_raw_ = nullptr;
    alloc->alloc_total_ = 0;
    alloc->pool_base_ = data;
    alloc->pool_size_ = data_size;
    alloc->owns_memory_ = false;

    for (uint8_t g = 0; g < NUM_SLOT_GROUPS; ++g) {
        for (uint8_t b = 0; b < 2; ++b)
            alloc->init_free_list(g, b);
        alloc->write_bank_[g].store(0, std::memory_order_relaxed);
        alloc->caches_[g].count = 0;
    }
    return alloc;
}

void quad_pong_allocator::destroy(quad_pong_allocator* alloc) noexcept {
    if (!alloc) return;
    if (alloc->owns_memory_) {
        VirtualUnlock(alloc->pool_raw_, alloc->alloc_total_);
        VirtualFree(alloc->pool_raw_, 0, MEM_RELEASE);
    } else {
        delete alloc;
    }
}

std::expected<void*, alloc_error>
quad_pong_allocator::allocate(uint8_t slot_group, uint16_t tag) {
    LocalCache& cache = caches_[slot_group];
    if (void* cached = CacheOps::try_pop(cache))
        return TaggedPtr::pack(cached, tag);

    uint8_t wb = write_bank_[slot_group].load(std::memory_order_acquire);
    CacheOps::fill(cache, banks_[slot_group][wb]);

    if (void* filled = CacheOps::try_pop(cache))
        return TaggedPtr::pack(filled, tag);

    return std::unexpected(alloc_error::pool_exhausted);
}

void quad_pong_allocator::deallocate(void* tagged_ptr) noexcept {
    void* real = TaggedPtr::untag(tagged_ptr);
    size_t offset = static_cast<char*>(real) - static_cast<char*>(pool_base_);
    size_t chunk_idx = offset / CHUNK_SIZE;
    uint8_t group = static_cast<uint8_t>(chunk_idx / (2 * BANK_CAPACITY));

    LocalCache& cache = caches_[group];
    if (cache.count == LOCAL_CACHE_SIZE) {
        uint8_t wb = write_bank_[group].load(std::memory_order_acquire);
        CacheOps::flush(cache, banks_[group][wb]);
    }
    cache.items[cache.count++] = real;
}

void quad_pong_allocator::swap_banks(uint8_t slot_group) noexcept {
    OCTOLITE_TRACE_INSTANT("swap_banks");
    uint8_t current = write_bank_[slot_group].load(std::memory_order_acquire);
    write_bank_[slot_group].store(current ^ 1, std::memory_order_release);
}

} // namespace octolite
