#include "../include/pch.hpp"
#include "../include/octolite.hpp"
#include "../include/vulkan_backend.hpp"
#include "../include/octolite_window.hpp"
#include "octolite_allocator.hpp"
#include <cstdio>
#include <cmath>
#include <chrono>
#include <random>

// Windows API'nin kodumuzu bozmasını kesin olarak engelliyoruz
#undef near
#undef far

using namespace octolite;

struct Mat4 {
    float m[16];
    static Mat4 identity() {
        Mat4 r{};
        r.m[0]=r.m[5]=r.m[10]=r.m[15]=1.0f;
        return r;
    }
    // Parametre isimlerini zNear ve zFar yaparak makro çakışmasını kökten çözdük
    static Mat4 perspective(float fov, float aspect, float zNear, float zFar) {
        Mat4 r{};
        float f = 1.0f / std::tan(fov * 0.5f);
        r.m[0] = f / aspect;
        r.m[5] = f;
        r.m[10] = (zFar + zNear) / (zNear - zFar);
        r.m[11] = -1.0f;
        r.m[14] = (2.0f * zFar * zNear) / (zNear - zFar);
        return r;
    }
    static Mat4 translate(float x, float y, float z) {
        Mat4 r = identity();
        r.m[12] = x; r.m[13] = y; r.m[14] = z;
        return r;
    }
    static Mat4 rotate_y(float angle) {
        Mat4 r = identity();
        float c = std::cos(angle), s = std::sin(angle);
        r.m[0] = c;  r.m[8] = s;
        r.m[2] = -s; r.m[10] = c;
        return r;
    }
    Mat4 operator*(const Mat4& o) const {
        Mat4 r{};
        for (int c = 0; c < 4; ++c)
            for (int row = 0; row < 4; ++row) {
                float sum = 0;
                for (int k = 0; k < 4; ++k)
                    sum += m[k*4+row] * o.m[c*4+k];
                r.m[c*4+row] = sum;
            }
        return r;
    }
};

void scatter_sphere(float* base, uint32_t chunk_count,
                    float cx, float cy, float cz, float radius,
                    std::mt19937& rng) {
    std::uniform_real_distribution<float> u(-1.0f, 1.0f);
    for (uint32_t c = 0; c < chunk_count; ++c) {
        float* chunk = base + c * 64;
        for (uint32_t l = 0; l < 4; ++l) {
            float x, y, z, len_sq;
            do {
                x = u(rng); y = u(rng); z = u(rng);
                len_sq = x*x + y*y + z*z;
            } while (len_sq > 1.0f || len_sq < 0.001f);
            float inv = radius / std::sqrt(len_sq);
            chunk[0 + l] = cx + x * inv;
            chunk[1 + l] = cy + y * inv;
            chunk[2 + l] = cz + z * inv;
            chunk[3 + l] = 0.0f;
            chunk[4 + l] = 0.0f;
            chunk[5 + l] = 0.0f;
        }
    }
}

int main() {
    // Window yerine güncellediğimiz OctoWindow türünü çağırıyoruz
    octolite::OctoWindow win = create_window(1280, 720, L"Octolite - Rotating Spheres");
    auto ctx = create_gpu_context(win.hwnd);
    auto sc = create_swapchain(ctx, win.width, win.height);

    std::printf("swapchain: %ux%u, %zu images\n",
                sc.extent.width, sc.extent.height, sc.images.size());

    constexpr size_t DATA_SIZE = NUM_SLOT_GROUPS * 2 * BANK_CAPACITY * CHUNK_SIZE;
    auto buf = create_buffer(ctx, DATA_SIZE,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    auto res = octolite::quad_pong_allocator::create_mapped(buf.mapped, DATA_SIZE);
    if (!res) return 1;
    auto* alloc = *res;

    constexpr uint32_t SPHERES = 3;
    constexpr uint32_t CHUNKS_PER_SPHERE = 8;
    constexpr uint32_t TOTAL_CHUNKS = SPHERES * CHUNKS_PER_SPHERE;
    constexpr uint32_t TOTAL_ENTITIES = TOTAL_CHUNKS * 16;

    float* base = static_cast<float*>(alloc->pool_base());
    std::mt19937 rng(42);

    scatter_sphere(base + 0 * CHUNKS_PER_SPHERE * 64,
                   CHUNKS_PER_SPHERE, -2.5f, 0.0f, 0.0f, 1.0f, rng);
    scatter_sphere(base + 1 * CHUNKS_PER_SPHERE * 64,
                   CHUNKS_PER_SPHERE, 0.0f, 0.0f, 0.0f, 1.4f, rng);
    scatter_sphere(base + 2 * CHUNKS_PER_SPHERE * 64,
                   CHUNKS_PER_SPHERE, 2.5f, 0.0f, 0.0f, 0.8f, rng);

    auto compute_pipe = create_integrate_pipeline(ctx);
    auto render_pipe = create_point_pipeline(ctx, sc.render_pass);
    auto runner = create_dispatch_runner(ctx);

    VkDeviceSize world_bytes = TOTAL_CHUNKS * CHUNK_SIZE;
    bind_buffer_offset(ctx, compute_pipe, buf, 0, world_bytes);
    bind_render_buffer(ctx, render_pipe, buf, 0, world_bytes);

    constexpr float DT = 1.0f / 60.0f;
    uint64_t frames = 0;
    auto fps_t0 = std::chrono::steady_clock::now();
    auto anim_t0 = fps_t0;

    while (pump_messages(win)) {
        auto now = std::chrono::steady_clock::now();
        float anim_t = std::chrono::duration<float>(now - anim_t0).count();

        float cam_angle = anim_t * 0.3f;
        float cam_r = 8.0f;
        float cam_x = std::cos(cam_angle) * cam_r;
        float cam_z = std::sin(cam_angle) * cam_r;
        float cam_y = 2.0f + std::sin(anim_t * 0.2f);

        float fx = -cam_x, fy = -cam_y, fz = -cam_z;
        float fl = std::sqrt(fx*fx + fy*fy + fz*fz);
        fx /= fl; fy /= fl; fz /= fl;
        float up_x = 0, up_y = 1, up_z = 0;

        float rx = fy*up_z - fz*up_y;
        float ry = fz*up_x - fx*up_z;
        float rz = fx*up_y - fy*up_x;
        float rl = std::sqrt(rx*rx + ry*ry + rz*rz);
        rx /= rl; ry /= rl; rz /= rl;

        float ux = ry*fz - rz*fy;
        float uy = rz*fx - rx*fz;
        float uz = rx*fy - ry*fx;

        Mat4 view = Mat4::identity();
        view.m[0]=rx; view.m[4]=ry; view.m[8]=rz;
        view.m[1]=ux; view.m[5]=uy; view.m[9]=uz;
        view.m[2]=-fx;view.m[6]=-fy;view.m[10]=-fz;
        view.m[12] = -(rx*cam_x + ry*cam_y + rz*cam_z);
        view.m[13] = -(ux*cam_x + uy*cam_y + uz*cam_z);
        view.m[14] = (fx*cam_x + fy*cam_y + fz*cam_z);

        float aspect = (float)sc.extent.width / (float)sc.extent.height;
        Mat4 proj = Mat4::perspective(1.0f, aspect, 0.1f, 100.0f);
        Mat4 vp = proj * view;

        float rot_speed = 0.5f;
        float cr = std::cos(rot_speed * DT);
        float sr = std::sin(rot_speed * DT);
        for (uint32_t c = 0; c < TOTAL_CHUNKS; ++c) {
            float* chunk = base + c * 64;
            for (uint32_t l = 0; l < 4; ++l) {
                float x = chunk[0 + l];
                float z = chunk[2 + l];
                chunk[0 + l] = x * cr - z * sr;
                chunk[2 + l] = x * sr + z * cr;
            }
        }

        auto fence = runner_dispatch(ctx, runner, compute_pipe, buf,
                                     TOTAL_CHUNKS, DT, 0, world_bytes);

        render_frame(ctx, sc, compute_pipe, render_pipe, runner, buf,
                     TOTAL_CHUNKS, TOTAL_ENTITIES, DT, vp.m);

        ++frames;
        double elapsed = std::chrono::duration<double>(now - fps_t0).count();
        if (elapsed >= 2.0) {
            std::printf("fps: %.1f | entities: %u\n",
                        frames / elapsed, TOTAL_ENTITIES);
            frames = 0;
            fps_t0 = now;
        }
    }

    vkDeviceWaitIdle(ctx.device);
    destroy_dispatch_runner(ctx, runner);
    destroy_render_pipeline(ctx, render_pipe);
    destroy_compute_pipeline(ctx, compute_pipe);
    quad_pong_allocator::destroy(alloc);
    destroy_buffer(ctx, buf);
    destroy_swapchain(ctx, sc);
    destroy_gpu_context(ctx);
    destroy_window(win);
    return 0;
}
