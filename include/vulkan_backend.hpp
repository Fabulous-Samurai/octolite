#pragma once
#include "octolite.hpp"
#include "octolite_window.hpp" // OctoWindow tanımını buradan güvenli şekilde almak için ekledik
#include <cstdint>
#include <vulkan/vulkan.h>
#include <vector>

namespace octolite {

struct GpuContext {
    VkInstance instance{VK_NULL_HANDLE};
    VkPhysicalDevice physical_device{VK_NULL_HANDLE};
    VkDevice device{VK_NULL_HANDLE};
    VkSurfaceKHR surface{VK_NULL_HANDLE};
    VkQueue graphics_queue{VK_NULL_HANDLE};
    VkQueue transfer_queue{VK_NULL_HANDLE};
    uint32_t graphics_family{0};
    uint32_t transfer_family{0};
    VkCommandPool command_pool{VK_NULL_HANDLE};
};

struct Buffer {
    VkBuffer buffer{VK_NULL_HANDLE};
    VkDeviceMemory memory{VK_NULL_HANDLE};
    void *mapped{nullptr};
    VkDeviceSize size{0};
};

struct UploadRing {
    Buffer buffers[2];
    uint32_t back{0};
    uint32_t chunk_capacity{0};
};

struct ComputePipeline {
    VkShaderModule module{VK_NULL_HANDLE};
    VkDescriptorSetLayout set_layout{VK_NULL_HANDLE};
    VkPipelineLayout pipeline_layout{VK_NULL_HANDLE};
    VkPipeline pipeline{VK_NULL_HANDLE};
    VkDescriptorPool descriptor_pool{VK_NULL_HANDLE};
    VkDescriptorSet descriptor_set{VK_NULL_HANDLE};
};

struct DispatchRunner {
    static constexpr uint32_t RING = 4;
    VkCommandBuffer cmds[RING]{};
    VkFence fences[RING]{};
    uint32_t next{0};
};

struct Swapchain {
    VkSwapchainKHR swapchain{VK_NULL_HANDLE};
    VkRenderPass render_pass{VK_NULL_HANDLE};
    VkFormat format{VK_FORMAT_UNDEFINED};
    VkExtent2D extent{};
    std::vector<VkImage> images;
    std::vector<VkImageView> views;
    std::vector<VkFramebuffer> framebuffers;
    std::vector<VkCommandBuffer> cmds;
    std::vector<VkFence> fences;
    VkSemaphore image_acquired{VK_NULL_HANDLE};
    VkSemaphore render_done{VK_NULL_HANDLE};
};

struct RenderPipeline {
    VkShaderModule vert{VK_NULL_HANDLE};
    VkShaderModule frag{VK_NULL_HANDLE};
    VkDescriptorSetLayout set_layout{VK_NULL_HANDLE};
    VkPipelineLayout pipeline_layout{VK_NULL_HANDLE};
    VkPipeline pipeline{VK_NULL_HANDLE};
    VkDescriptorPool descriptor_pool{VK_NULL_HANDLE};
    VkDescriptorSet descriptor_set{VK_NULL_HANDLE};
};

// ÇAKIŞAN ESKİ WINDOW MÜKERRER BİLDİRİMLERİ BURADAN TAMAMEN KALDIRILDI

// GPU context
GpuContext create_gpu_context(HWND hwnd);
void destroy_gpu_context(GpuContext &ctx);

// Buffer management
uint32_t find_memory_type(const GpuContext &ctx, uint32_t type_filter, VkMemoryPropertyFlags properties);
Buffer create_buffer(const GpuContext &ctx, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);
void destroy_buffer(const GpuContext &ctx, Buffer &buf);

// Upload ring
UploadRing create_upload_ring(const GpuContext &ctx, uint32_t chunk_capacity);
void destroy_upload_ring(const GpuContext &ctx, UploadRing &ring);
void *ring_back(const UploadRing &ring) noexcept;
void *ring_front(const UploadRing &ring) noexcept;
void ring_swap(UploadRing &ring) noexcept;

// Compute pipeline
ComputePipeline create_integrate_pipeline(const GpuContext &ctx);
void destroy_compute_pipeline(const GpuContext &ctx, ComputePipeline &p);
void bind_buffer(const GpuContext &ctx, ComputePipeline &p, const Buffer &buf);
void dispatch_buffer(const GpuContext &ctx, ComputePipeline &p, const Buffer &buf, uint32_t chunk_count, float dt);
void bind_ring_front(const GpuContext &ctx, ComputePipeline &p, const UploadRing &ring);
void dispatch_integrate(const GpuContext &ctx, ComputePipeline &p, const UploadRing &ring, uint32_t chunk_count, float dt);
void bind_buffer_offset(const GpuContext &ctx, ComputePipeline &p, const Buffer &buf, VkDeviceSize offset, VkDeviceSize range);
VkFence dispatch_buffer_async(const GpuContext &ctx, ComputePipeline &p, const Buffer &buf, uint32_t chunk_count, float dt, VkDeviceSize offset, VkDeviceSize range);

// Dispatch runner
DispatchRunner create_dispatch_runner(const GpuContext &ctx);
void destroy_dispatch_runner(const GpuContext &ctx, DispatchRunner &runner);
VkFence runner_dispatch(const GpuContext &ctx, DispatchRunner &runner, ComputePipeline &p, const Buffer &buf, uint32_t chunk_count, float dt, VkDeviceSize offset, VkDeviceSize range);
void runner_wait(const GpuContext &ctx, DispatchRunner &runner, VkFence fence);

// Swapchain
Swapchain create_swapchain(const GpuContext &ctx, uint32_t width, uint32_t height);
void destroy_swapchain(const GpuContext &ctx, Swapchain &sc);
void clear_and_present(const GpuContext &ctx, Swapchain &sc);

// Render pipeline
RenderPipeline create_point_pipeline(const GpuContext &ctx, VkRenderPass render_pass);
void destroy_render_pipeline(const GpuContext &ctx, RenderPipeline &p);
void bind_render_buffer(const GpuContext &ctx, RenderPipeline &p, const Buffer &buf, VkDeviceSize offset, VkDeviceSize range);

// Render frame with VP matrix
void render_frame(const GpuContext &ctx, Swapchain &sc, ComputePipeline &cp, RenderPipeline &rp, DispatchRunner &runner, const Buffer &buf, uint32_t chunk_count, uint32_t entity_count, float dt, const float *vp_data);

} // namespace octolite
