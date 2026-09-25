#include "../include/pch.hpp"
#include "octolite_chunk_pool.hpp"

namespace octolite {

void ChunkPool::bind(quad_pong_allocator* alloc, uint8_t slot_group, uint32_t initial_capacity) {
    alloc_ = alloc;
    slot_group_ = slot_group;
    chunks_.reserve(initial_capacity);
    masks_.reserve(initial_capacity);
}

Chunk* ChunkPool::at(size_t idx) noexcept {
    if (idx >= chunks_.size()) return nullptr;
    return chunks_[idx];
}

uint32_t ChunkPool::used_mask(size_t idx) const noexcept {
    if (idx >= masks_.size()) return 0;
    return masks_[idx];
}

} // namespace octolite
