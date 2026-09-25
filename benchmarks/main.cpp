#include "../include/pch.hpp"
#include "../include/octolite.hpp"
#include "octolite_allocator.hpp"
#include <thread>
#include <vector>
#include <chrono>
#include <cstdio>

int main() {
    std::printf("=== Multi-threaded Allocator Stress Test ===\n");

    auto res = octolite::quad_pong_allocator::create(256 * 1024 * 1024);
    if (!res) {
        std::printf("create failed\n");
        return 1;
    }
    auto* alloc = *res;

    constexpr int NUM_THREADS = 8;
    constexpr int ALLOCS_PER_THREAD = 100000;

    auto t0 = std::chrono::steady_clock::now();

    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};

    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([&, t]() {
            int local_ok = 0;
            for (int i = 0; i < ALLOCS_PER_THREAD; ++i) {
                auto r = alloc->allocate(static_cast<uint8_t>(t % 4), 1);
                if (r) {
                    ++local_ok;
                    alloc->deallocate(*r);
                }
            }
            success_count.fetch_add(local_ok);
        });
    }
    for (auto& th : threads) th.join();

    auto t1 = std::chrono::steady_clock::now();
    double dt = std::chrono::duration<double>(t1 - t0).count();
    int64_t total = (int64_t)NUM_THREADS * ALLOCS_PER_THREAD;
    std::printf("threads: %d, allocs/thread: %d\n", NUM_THREADS, ALLOCS_PER_THREAD);
    std::printf("total ops: %lld in %.3fs\n", (long long)total, dt);
    std::printf("throughput: %.2f M ops/sec\n", total / dt / 1e6);
    std::printf("successful: %d / %lld (%.1f%%)\n",
                success_count.load(), (long long)total,
                100.0 * success_count.load() / total);

    octolite::quad_pong_allocator::destroy(alloc);
    return 0;
}
