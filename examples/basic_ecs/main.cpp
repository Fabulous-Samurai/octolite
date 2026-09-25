#include "../include/pch.hpp"
#include "../include/octolite.hpp"
#include "octolite_allocator.hpp"
#include <cstdio>

int main() {
    std::printf("=== Octolite Basic ECS Demo ===\n");

    // 64 MB pool tahsis et
    auto res = octolite::quad_pong_allocator::create(64 * 1024 * 1024);
    if (!res) {
        std::printf("allocator create failed\n");
        return 1;
    }
    auto* alloc = *res;

    // 100 entity allocate et
    for (int i = 0; i < 100; ++i) {
        auto r = alloc->allocate(0, static_cast<uint16_t>(i));
        if (r) {
            void* ptr = octolite::TaggedPtr::untag(*r);
            std::printf("entity %d @ %p (tag=%u)\n", i, ptr, octolite::TaggedPtr::tag_of(*r));
        } else {
            std::printf("entity %d alloc failed\n", i);
        }
    }

    octolite::quad_pong_allocator::destroy(alloc);
    std::printf("Done.\n");
    return 0;
}
