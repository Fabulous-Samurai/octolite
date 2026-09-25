#pragma once
#include "octolite.hpp"
#include <expected>
#include <cstdint>

namespace octolite {

enum class alloc_error : uint8_t {
    virtual_alloc_failed = 1,
    pool_exhausted = 2
};

class quad_pong_allocator {
    Bank banks_[NUM_SLOT_GROUPS][2];
    LocalCache caches_[NUM_SLOT_GROUPS];
    void* pool_raw_{nullptr};
    void* pool_base_{nullptr};
    size_t pool_size_{0};
    size_t alloc_total_{0};
    std::atomic<uint8_t> write_bank_[NUM_SLOT_GROUPS];
    bool owns_memory_{false};

    FreeNode* node_at(uint8_t group, uint8_t bank, size_t idx) noexcept;
    void init_free_list(uint8_t group, uint8_t bank) noexcept;

public:
    static std::expected<quad_pong_allocator*, alloc_error> create(size_t pool_size);
    static std::expected<quad_pong_allocator*, alloc_error> create_mapped(void* data, size_t data_size);
    static void destroy(quad_pong_allocator* alloc) noexcept;

    std::expected<void*, alloc_error> allocate(uint8_t slot_group, uint16_t tag);
    void deallocate(void* tagged_ptr) noexcept;
    void swap_banks(uint8_t slot_group) noexcept;
    void* pool_base() const noexcept { return pool_base_; }
};

} // namespace octolite
