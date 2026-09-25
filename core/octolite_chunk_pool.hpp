#pragma once
#include "../include/octolite.hpp"
#include "../include/octolite_allocator.hpp"

namespace octolite {

struct Chunk {
    float pos_x[4], pos_y[4], pos_z[4];
    float vel_x[4], vel_y[4], vel_z[4];
    uint32_t mask; // Active lane mask
};

class ChunkPool {
    quad_pong_allocator* alloc_;
    uint8_t slot_group_;
    std::vector<Chunk*> chunks_;
    std::vector<uint32_t> masks_;

public:
    void bind(quad_pong_allocator* alloc, uint8_t slot_group, uint32_t initial_capacity);
    Chunk* at(size_t idx) noexcept;
    uint32_t used_mask(size_t idx) const noexcept;
    size_t count() const noexcept { return chunks_.size(); }
};

} // namespace octolite
