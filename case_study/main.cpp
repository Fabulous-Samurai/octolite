#include "../include/pch.hpp"
#include "../include/octolite_gpu.hpp"
#include "../include/octolite_window.hpp"
#include "../include/octolite_allocator.hpp"
#include "../core/octolite_chunk_pool.hpp"
#include <cstdio>
#include <chrono>

int main() {
    auto win = octolite::create_window(1280, 720, L"Octolite Demo");
    auto ctx = octolite::create_gpu_context(win.hwnd);
    auto sc = octolite::create_swapchain(ctx, win.width, win.height);

    constexpr size_t DATA_SIZE = octolite::NUM_SLOT_GROUPS * 2 * octolite::BANK_CAPACITY * octolite::CHUNK_SIZE;
    auto buf = octolite::create_buffer(ctx, DATA_SIZE,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    auto res = octolite::quad_pong_allocator::create_mapped(buf.mapped, DATA_SIZE);
    if (!res) return 1;
    auto* alloc = *res;

    octolite::ChunkPool world_pool;
    world_pool.bind(alloc, 0, 1024);

    auto compute_pipe = octolite::create_integrate_pipeline(ctx);
    auto render_pipe = octolite::create_point_pipeline(ctx, sc.render_pass);
    auto runner = octolite::create_dispatch_runner(ctx);

    VkDeviceSize world_bytes = world_pool.count() * octolite::CHUNK_SIZE;
    octolite::bind_buffer_offset(ctx, compute_pipe, buf, 0, world_bytes);
    octolite::bind_render_buffer(ctx, render_pipe, buf, 0, world_bytes);

    constexpr float DT = 1.0f / 60.0f;
    uint64_t frames = 0;
    auto fps_t0 = std::chrono::steady_clock::now();

    // Identity VP Matrix for now
    float identity_vp[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};

    while (octolite::pump_messages(win)) {
        auto fence = octolite::runner_dispatch(ctx, runner, compute_pipe, buf,
            static_cast<uint32_t>(world_pool.count()), DT, 0, world_bytes);

        // VP matrix parametresi eklendi
        octolite::render_frame(ctx, sc, compute_pipe, render_pipe, runner, buf,
            static_cast<uint32_t>(world_pool.count()), 4096, DT, identity_vp);

        ++frames;
        auto now = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(now - fps_t0).count();
        if (elapsed >= 2.0) {
            std::printf("fps: %.1f\n", frames / elapsed);
            frames = 0;
            fps_t0 = now;
        }
    }

    vkDeviceWaitIdle(ctx.device);
    octolite::destroy_dispatch_runner(ctx, runner);
    octolite::destroy_render_pipeline(ctx, render_pipe);
    octolite::destroy_compute_pipeline(ctx, compute_pipe);
    octolite::quad_pong_allocator::destroy(alloc);
    octolite::destroy_buffer(ctx, buf);
    octolite::destroy_swapchain(ctx, sc);
    octolite::destroy_gpu_context(ctx);
    octolite::destroy_window(win);
    return 0;
}
