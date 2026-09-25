#pragma once
#include "../include/octolite.hpp"
#include <vector>
#include <cstdint>

namespace octolite {

class Registry {
    std::vector<uint32_t> alive_entities_;
    std::vector<bool> dirty_flags_;
    uint32_t next_id_{0};

public:
    void begin_frame();
    uint32_t create();
    void destroy(uint32_t id);
    void end_frame();

    const std::vector<uint32_t>& alive() const { return alive_entities_; }
    size_t count() const { return alive_entities_.size(); }
};

} // namespace octolite
