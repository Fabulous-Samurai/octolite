#include "pch.hpp"
#include "octolite_gpu.hpp"
#include <cstdio>
#include <cstring>
#include <stdexcept>

namespace octolite {

GpuContext create_gpu_context(HWND hwnd) {
    GpuContext ctx;
    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "octolite";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "octolite";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_3;

    const char *instance_exts[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME
    };

    VkInstanceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;
    create_info.enabledExtensionCount = 2;
    create_info.ppEnabledExtensionNames = instance_exts;

    if (vkCreateInstance(&create_info, nullptr, &ctx.instance) != VK_SUCCESS)
        throw std::runtime_error("failed to create Vulkan instance");

    if (hwnd) {
        VkWin32SurfaceCreateInfoKHR sci{};
        sci.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        sci.hinstance = GetModuleHandleW(nullptr);
        sci.hwnd = hwnd;
        if (vkCreateWin32SurfaceKHR(ctx.instance, &sci, nullptr, &ctx.surface) != VK_SUCCESS)
            throw std::runtime_error("failed to create win32 surface");
    }

    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(ctx.instance, &device_count, nullptr);
    if (device_count == 0)
        throw std::runtime_error("no Vulkan-capable GPU found");

    std::vector<VkPhysicalDevice> devices(device_count);
    vkEnumeratePhysicalDevices(ctx.instance, &device_count, devices.data());
    ctx.physical_device = devices[0];

    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(ctx.physical_device, &queue_family_count, nullptr);
    std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(ctx.physical_device, &queue_family_count, queue_families.data());

    bool found_graphics = false, found_transfer = false;
    for (uint32_t i = 0; i < queue_family_count; ++i) {
        bool present_ok = true;
        if (ctx.surface) {
            VkBool32 ok = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(ctx.physical_device, i, ctx.surface, &ok);
            present_ok = (ok == VK_TRUE);
        }
        if ((queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && present_ok) {
            ctx.graphics_family = i;
            found_graphics = true;
        }
        if ((queue_families[i].queueFlags & VK_QUEUE_TRANSFER_BIT) &&
            !(queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
            ctx.transfer_family = i;
            found_transfer = true;
        }
    }

    if (!found_graphics)
        throw std::runtime_error("no graphics queue found");
    if (!found_transfer)
        ctx.transfer_family = ctx.graphics_family;

    std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
    float queue_priority = 1.0f;

    VkDeviceQueueCreateInfo graphics_queue_info{};
    graphics_queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    graphics_queue_info.queueFamilyIndex = ctx.graphics_family;
    graphics_queue_info.queueCount = 1;
    graphics_queue_info.pQueuePriorities = &queue_priority;
    queue_create_infos.push_back(graphics_queue_info);

    if (ctx.transfer_family != ctx.graphics_family) {
        VkDeviceQueueCreateInfo transfer_queue_info{};
        transfer_queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        transfer_queue_info.queueFamilyIndex = ctx.transfer_family;
        transfer_queue_info.queueCount = 1;
        transfer_queue_info.pQueuePriorities = &queue_priority;
        queue_create_infos.push_back(transfer_queue_info);
    }

    const char *device_exts[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    VkDeviceCreateInfo device_info{};
    device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_info.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size());
    device_info.pQueueCreateInfos = queue_create_infos.data();
    device_info.enabledExtensionCount = 1;
    device_info.ppEnabledExtensionNames = device_exts;

    if (vkCreateDevice(ctx.physical_device, &device_info, nullptr, &ctx.device) != VK_SUCCESS)
        throw std::runtime_error("failed to create logical device");

    vkGetDeviceQueue(ctx.device, ctx.graphics_family, 0, &ctx.graphics_queue);
    vkGetDeviceQueue(ctx.device, ctx.transfer_family, 0, &ctx.transfer_queue);

    VkCommandPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.queueFamilyIndex = ctx.graphics_family;
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    if (vkCreateCommandPool(ctx.device, &pool_info, nullptr, &ctx.command_pool) != VK_SUCCESS)
        throw std::runtime_error("failed to create command pool");

    return ctx;
}

void destroy_gpu_context(GpuContext &ctx) {
    if (ctx.command_pool) vkDestroyCommandPool(ctx.device, ctx.command_pool, nullptr);
    if (ctx.device) vkDestroyDevice(ctx.device, nullptr);
    if (ctx.surface) vkDestroySurfaceKHR(ctx.instance, ctx.surface, nullptr);
    if (ctx.instance) vkDestroyInstance(ctx.instance, nullptr);
}

uint32_t find_memory_type(const GpuContext &ctx, uint32_t type_filter,
                          VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties mem_props;
    vkGetPhysicalDeviceMemoryProperties(ctx.physical_device, &mem_props);
    for (uint32_t i = 0; i < mem_props.memoryTypeCount; ++i) {
        if ((type_filter & (1u << i)) &&
            (mem_props.memoryTypes[i].propertyFlags & properties) == properties)
            return i;
    }
    throw std::runtime_error("no suitable memory type found");
}

Buffer create_buffer(const GpuContext &ctx, VkDeviceSize size,
                     VkBufferUsageFlags usage, VkMemoryPropertyFlags properties) {
    Buffer buf;
    buf.size = size;
    VkBufferCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    info.size = size;
    info.usage = usage;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(ctx.device, &info, nullptr, &buf.buffer) != VK_SUCCESS)
        throw std::runtime_error("failed to create buffer");
    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(ctx.device, buf.buffer, &req);
    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = req.size;
    alloc_info.memoryTypeIndex = find_memory_type(ctx, req.memoryTypeBits, properties);
    if (vkAllocateMemory(ctx.device, &alloc_info, nullptr, &buf.memory) != VK_SUCCESS)
        throw std::runtime_error("failed to allocate buffer memory");
    vkBindBufferMemory(ctx.device, buf.buffer, buf.memory, 0);
    if (properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
        if (vkMapMemory(ctx.device, buf.memory, 0, size, 0, &buf.mapped) != VK_SUCCESS)
            throw std::runtime_error("failed to map buffer memory");
    }
    return buf;
}

void destroy_buffer(const GpuContext &ctx, Buffer &buf) {
    if (buf.mapped && buf.memory)
        vkUnmapMemory(ctx.device, buf.memory);
    if (buf.buffer) vkDestroyBuffer(ctx.device, buf.buffer, nullptr);
    if (buf.memory) vkFreeMemory(ctx.device, buf.memory, nullptr);
    buf = {};
}

UploadRing create_upload_ring(const GpuContext &ctx, uint32_t chunk_capacity) {
    UploadRing ring;
    ring.chunk_capacity = chunk_capacity;
    VkDeviceSize size = static_cast<VkDeviceSize>(chunk_capacity) * CHUNK_SIZE;
    VkBufferUsageFlags usage =
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    VkMemoryPropertyFlags props =
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    ring.buffers[0] = create_buffer(ctx, size, usage, props);
    ring.buffers[1] = create_buffer(ctx, size, usage, props);
    ring.back = 0;
    return ring;
}

void destroy_upload_ring(const GpuContext &ctx, UploadRing &ring) {
    destroy_buffer(ctx, ring.buffers[0]);
    destroy_buffer(ctx, ring.buffers[1]);
}

void *ring_back(const UploadRing &ring) noexcept {
    return ring.buffers[ring.back].mapped;
}

void *ring_front(const UploadRing &ring) noexcept {
    return ring.buffers[ring.back ^ 1].mapped;
}

void ring_swap(UploadRing &ring) noexcept {
    ring.back ^= 1;
}

static const unsigned char integrate_spv[] = {
    #embed "../assets/shaders/integrate.spv"
};
static const unsigned char render_vert_spv[] = {
#embed "../assets/shaders/render.vert.spv"
};
static const unsigned char render_frag_spv[] = {
#embed "../assets/shaders/render.frag.spv"
};

ComputePipeline create_integrate_pipeline(const GpuContext &ctx) {
    ComputePipeline p;
    VkShaderModuleCreateInfo mod_info{};
    mod_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    mod_info.codeSize = sizeof(integrate_spv);
    mod_info.pCode = reinterpret_cast<const uint32_t *>(integrate_spv);
    if (vkCreateShaderModule(ctx.device, &mod_info, nullptr, &p.module) != VK_SUCCESS)
        throw std::runtime_error("failed to create shader module");
    VkDescriptorSetLayoutBinding binding{};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    VkDescriptorSetLayoutCreateInfo layout_info{};
    layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.bindingCount = 1;
    layout_info.pBindings = &binding;
    if (vkCreateDescriptorSetLayout(ctx.device, &layout_info, nullptr, &p.set_layout) != VK_SUCCESS)
        throw std::runtime_error("failed to create descriptor set layout");
    VkPushConstantRange push_range{};
    push_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    push_range.offset = 0;
    push_range.size = 8;
    VkPipelineLayoutCreateInfo pl_info{};
    pl_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pl_info.setLayoutCount = 1;
    pl_info.pSetLayouts = &p.set_layout;
    pl_info.pushConstantRangeCount = 1;
    pl_info.pPushConstantRanges = &push_range;
    if (vkCreatePipelineLayout(ctx.device, &pl_info, nullptr, &p.pipeline_layout) != VK_SUCCESS)
        throw std::runtime_error("failed to create pipeline layout");
    VkComputePipelineCreateInfo pipe_info{};
    pipe_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipe_info.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipe_info.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipe_info.stage.module = p.module;
    pipe_info.stage.pName = "main";
    pipe_info.layout = p.pipeline_layout;
    if (vkCreateComputePipelines(ctx.device, VK_NULL_HANDLE, 1, &pipe_info, nullptr, &p.pipeline) != VK_SUCCESS)
        throw std::runtime_error("failed to create compute pipeline");
    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.maxSets = 1;
    VkDescriptorPoolSize pool_size{};
    pool_size.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    pool_size.descriptorCount = 1;
    pool_info.poolSizeCount = 1;
    pool_info.pPoolSizes = &pool_size;
    if (vkCreateDescriptorPool(ctx.device, &pool_info, nullptr, &p.descriptor_pool) != VK_SUCCESS)
        throw std::runtime_error("failed to create descriptor pool");
    VkDescriptorSetAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = p.descriptor_pool;
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts = &p.set_layout;
    if (vkAllocateDescriptorSets(ctx.device, &alloc_info, &p.descriptor_set) != VK_SUCCESS)
        throw std::runtime_error("failed to allocate descriptor set");
    return p;
}

void destroy_compute_pipeline(const GpuContext &ctx, ComputePipeline &p) {
    if (p.descriptor_pool) vkDestroyDescriptorPool(ctx.device, p.descriptor_pool, nullptr);
    if (p.pipeline) vkDestroyPipeline(ctx.device, p.pipeline, nullptr);
    if (p.pipeline_layout) vkDestroyPipelineLayout(ctx.device, p.pipeline_layout, nullptr);
    if (p.set_layout) vkDestroyDescriptorSetLayout(ctx.device, p.set_layout, nullptr);
    if (p.module) vkDestroyShaderModule(ctx.device, p.module, nullptr);
    p = {};
}

void bind_buffer(const GpuContext &ctx, ComputePipeline &p, const Buffer &buf) {
    VkDescriptorBufferInfo buf_info{};
    buf_info.buffer = buf.buffer;
    buf_info.offset = 0;
    buf_info.range = buf.size;
    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = p.descriptor_set;
    write.dstBinding = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    write.pBufferInfo = &buf_info;
    vkUpdateDescriptorSets(ctx.device, 1, &write, 0, nullptr);
}

void dispatch_buffer(const GpuContext &ctx, ComputePipeline &p, const Buffer &buf,
                     uint32_t chunk_count, float dt) {
    VkCommandBufferAllocateInfo cmd_info{};
    cmd_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmd_info.commandPool = ctx.command_pool;
    cmd_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmd_info.commandBufferCount = 1;
    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(ctx.device, &cmd_info, &cmd);
    VkCommandBufferBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &begin);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, p.pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                            p.pipeline_layout, 0, 1, &p.descriptor_set, 0, nullptr);
    struct { float dt; uint32_t count; } push{dt, chunk_count};
    vkCmdPushConstants(cmd, p.pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT,
                       0, sizeof(push), &push);
    vkCmdDispatch(cmd, (chunk_count + 63) / 64, 1, 1);
    VkBufferMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    barrier.buffer = buf.buffer;
    barrier.offset = 0;
    barrier.size = VK_WHOLE_SIZE;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_HOST_BIT, 0,
                         0, nullptr, 1, &barrier, 0, nullptr);
    vkEndCommandBuffer(cmd);
    VkFenceCreateInfo fence_info{};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VkFence fence;
    vkCreateFence(ctx.device, &fence_info, nullptr, &fence);
    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;
    vkQueueSubmit(ctx.graphics_queue, 1, &submit, fence);
    vkWaitForFences(ctx.device, 1, &fence, VK_TRUE, UINT64_MAX);
    vkDestroyFence(ctx.device, fence, nullptr);
    vkFreeCommandBuffers(ctx.device, ctx.command_pool, 1, &cmd);
}

void bind_ring_front(const GpuContext &ctx, ComputePipeline &p, const UploadRing &ring) {
    bind_buffer(ctx, p, ring.buffers[ring.back ^ 1]);
}

void dispatch_integrate(const GpuContext &ctx, ComputePipeline &p,
                        const UploadRing &ring, uint32_t chunk_count, float dt) {
    dispatch_buffer(ctx, p, ring.buffers[ring.back ^ 1], chunk_count, dt);
}

void bind_buffer_offset(const GpuContext &ctx, ComputePipeline &p, const Buffer &buf,
                        VkDeviceSize offset, VkDeviceSize range) {
    VkDescriptorBufferInfo buf_info{};
    buf_info.buffer = buf.buffer;
    buf_info.offset = offset;
    buf_info.range = range;
    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = p.descriptor_set;
    write.dstBinding = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    write.pBufferInfo = &buf_info;
    vkUpdateDescriptorSets(ctx.device, 1, &write, 0, nullptr);
}

VkFence dispatch_buffer_async(const GpuContext &ctx, ComputePipeline &p, const Buffer &buf,
                              uint32_t chunk_count, float dt,
                              VkDeviceSize offset, VkDeviceSize range) {
    bind_buffer_offset(ctx, p, buf, offset, range);
    VkCommandBufferAllocateInfo cmd_info{};
    cmd_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmd_info.commandPool = ctx.command_pool;
    cmd_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmd_info.commandBufferCount = 1;
    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(ctx.device, &cmd_info, &cmd);
    VkCommandBufferBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &begin);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, p.pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                            p.pipeline_layout, 0, 1, &p.descriptor_set, 0, nullptr);
    struct { float dt; uint32_t count; } push{dt, chunk_count};
    vkCmdPushConstants(cmd, p.pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT,
                       0, sizeof(push), &push);
    vkCmdDispatch(cmd, (chunk_count + 63) / 64, 1, 1);
    vkEndCommandBuffer(cmd);
    VkFenceCreateInfo fence_info{};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VkFence fence;
    vkCreateFence(ctx.device, &fence_info, nullptr, &fence);
    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;
    vkQueueSubmit(ctx.graphics_queue, 1, &submit, fence);
    vkFreeCommandBuffers(ctx.device, ctx.command_pool, 1, &cmd);
    return fence;
}

DispatchRunner create_dispatch_runner(const GpuContext &ctx) {
    DispatchRunner r;
    VkCommandBufferAllocateInfo cmd_info{};
    cmd_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmd_info.commandPool = ctx.command_pool;
    cmd_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmd_info.commandBufferCount = DispatchRunner::RING;
    if (vkAllocateCommandBuffers(ctx.device, &cmd_info, r.cmds) != VK_SUCCESS)
        throw std::runtime_error("failed to allocate runner command buffers");
    VkFenceCreateInfo fence_info{};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    for (uint32_t i = 0; i < DispatchRunner::RING; ++i) {
        if (vkCreateFence(ctx.device, &fence_info, nullptr, &r.fences[i]) != VK_SUCCESS)
            throw std::runtime_error("failed to create runner fence");
    }
    return r;
}

void destroy_dispatch_runner(const GpuContext &ctx, DispatchRunner &runner) {
    vkDeviceWaitIdle(ctx.device);
    for (uint32_t i = 0; i < DispatchRunner::RING; ++i)
        if (runner.fences[i]) vkDestroyFence(ctx.device, runner.fences[i], nullptr);
    vkFreeCommandBuffers(ctx.device, ctx.command_pool, DispatchRunner::RING, runner.cmds);
    runner = {};
}

VkFence runner_dispatch(const GpuContext &ctx, DispatchRunner &runner, ComputePipeline &p,
                        const Buffer &buf, uint32_t chunk_count, float dt,
                        VkDeviceSize offset, VkDeviceSize range) {
    uint32_t slot = runner.next;
    runner.next = (runner.next + 1) % DispatchRunner::RING;
    vkWaitForFences(ctx.device, 1, &runner.fences[slot], VK_TRUE, UINT64_MAX);
    vkResetFences(ctx.device, 1, &runner.fences[slot]);
    bind_buffer_offset(ctx, p, buf, offset, range);
    VkCommandBuffer cmd = runner.cmds[slot];
    vkResetCommandBuffer(cmd, 0);
    VkCommandBufferBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &begin);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, p.pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                            p.pipeline_layout, 0, 1, &p.descriptor_set, 0, nullptr);
    struct { float dt; uint32_t count; } push{dt, chunk_count};
    vkCmdPushConstants(cmd, p.pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT,
                       0, sizeof(push), &push);
    vkCmdDispatch(cmd, (chunk_count + 63) / 64, 1, 1);
    vkEndCommandBuffer(cmd);
    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;
    vkQueueSubmit(ctx.graphics_queue, 1, &submit, runner.fences[slot]);
    return runner.fences[slot];
}

void runner_wait(const GpuContext &ctx, DispatchRunner &runner, VkFence fence) {
    (void) runner;
    vkWaitForFences(ctx.device, 1, &fence, VK_TRUE, UINT64_MAX);
}

Swapchain create_swapchain(const GpuContext& ctx, uint32_t width, uint32_t height) {
    Swapchain sc;
    VkSurfaceCapabilitiesKHR caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(ctx.physical_device, ctx.surface, &caps);
    uint32_t fmt_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(ctx.physical_device, ctx.surface, &fmt_count, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(fmt_count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(ctx.physical_device, ctx.surface, &fmt_count, formats.data());
    sc.format = formats[0].format;
    for (auto& f : formats) {
        if (f.format == VK_FORMAT_B8G8R8A8_UNORM || f.format == VK_FORMAT_B8G8R8A8_SRGB) {
            sc.format = f.format;
            break;
        }
    }
    VkColorSpaceKHR color_space = formats[0].colorSpace;
    uint32_t pm_count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(ctx.physical_device, ctx.surface, &pm_count, nullptr);
    std::vector<VkPresentModeKHR> present_modes(pm_count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(ctx.physical_device, ctx.surface, &pm_count, present_modes.data());
    VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;
    for (auto pm : present_modes)
        if (pm == VK_PRESENT_MODE_MAILBOX_KHR) present_mode = pm;
    sc.extent = caps.currentExtent;
    if (sc.extent.width == 0xFFFFFFFF) {
        sc.extent.width = width;
        sc.extent.height = height;
    }
    uint32_t image_count = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && image_count > caps.maxImageCount)
        image_count = caps.maxImageCount;
    VkSwapchainCreateInfoKHR sci{};
    sci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    sci.surface = ctx.surface;
    sci.minImageCount = image_count;
    sci.imageFormat = sc.format;
    sci.imageColorSpace = color_space;
    sci.imageExtent = sc.extent;
    sci.imageArrayLayers = 1;
    sci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    sci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    sci.preTransform = caps.currentTransform;
    sci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    sci.presentMode = present_mode;
    sci.clipped = VK_TRUE;
    if (vkCreateSwapchainKHR(ctx.device, &sci, nullptr, &sc.swapchain) != VK_SUCCESS)
        throw std::runtime_error("failed to create swapchain");
    uint32_t got = 0;
    vkGetSwapchainImagesKHR(ctx.device, sc.swapchain, &got, nullptr);
    sc.images.resize(got);
    vkGetSwapchainImagesKHR(ctx.device, sc.swapchain, &got, sc.images.data());
    sc.views.resize(got);
    for (uint32_t i = 0; i < got; ++i) {
        VkImageViewCreateInfo iv{};
        iv.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        iv.image = sc.images[i];
        iv.viewType = VK_IMAGE_VIEW_TYPE_2D;
        iv.format = sc.format;
        iv.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        iv.subresourceRange.levelCount = 1;
        iv.subresourceRange.layerCount = 1;
        if (vkCreateImageView(ctx.device, &iv, nullptr, &sc.views[i]) != VK_SUCCESS)
            throw std::runtime_error("failed to create image view");
    }
    VkAttachmentDescription att{};
    att.format = sc.format;
    att.samples = VK_SAMPLE_COUNT_1_BIT;
    att.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    att.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    att.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    att.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    att.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    att.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    VkAttachmentReference color_ref{};
    color_ref.attachment = 0;
    color_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    VkSubpassDescription sub{};
    sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sub.colorAttachmentCount = 1;
    sub.pColorAttachments = &color_ref;
    VkSubpassDependency dep{};
    dep.srcSubpass = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass = 0;
    dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.srcAccessMask = 0;
    dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    VkRenderPassCreateInfo rpi{};
    rpi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpi.attachmentCount = 1;
    rpi.pAttachments = &att;
    rpi.subpassCount = 1;
    rpi.pSubpasses = &sub;
    rpi.dependencyCount = 1;
    rpi.pDependencies = &dep;
    if (vkCreateRenderPass(ctx.device, &rpi, nullptr, &sc.render_pass) != VK_SUCCESS)
        throw std::runtime_error("failed to create render pass");
    sc.framebuffers.resize(got);
    for (uint32_t i = 0; i < got; ++i) {
        VkFramebufferCreateInfo fbi{};
        fbi.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbi.renderPass = sc.render_pass;
        fbi.attachmentCount = 1;
        fbi.pAttachments = &sc.views[i];
        fbi.width = sc.extent.width;
        fbi.height = sc.extent.height;
        fbi.layers = 1;
        if (vkCreateFramebuffer(ctx.device, &fbi, nullptr, &sc.framebuffers[i]) != VK_SUCCESS)
            throw std::runtime_error("failed to create framebuffer");
    }
    VkCommandBufferAllocateInfo cai{};
    cai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cai.commandPool = ctx.command_pool;
    cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cai.commandBufferCount = got;
    sc.cmds.resize(got);
    if (vkAllocateCommandBuffers(ctx.device, &cai, sc.cmds.data()) != VK_SUCCESS)
        throw std::runtime_error("failed to allocate swapchain command buffers");
    sc.fences.resize(got);
    VkFenceCreateInfo fci{};
    fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    for (uint32_t i = 0; i < got; ++i)
        if (vkCreateFence(ctx.device, &fci, nullptr, &sc.fences[i]) != VK_SUCCESS)
            throw std::runtime_error("failed to create swapchain fence");
    VkSemaphoreCreateInfo si{};
    si.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    if (vkCreateSemaphore(ctx.device, &si, nullptr, &sc.image_acquired) != VK_SUCCESS)
        throw std::runtime_error("failed to create semaphore");
    if (vkCreateSemaphore(ctx.device, &si, nullptr, &sc.render_done) != VK_SUCCESS)
        throw std::runtime_error("failed to create semaphore");
    return sc;
}

void destroy_swapchain(const GpuContext& ctx, Swapchain& sc) {
    vkDeviceWaitIdle(ctx.device);
    if (sc.image_acquired) vkDestroySemaphore(ctx.device, sc.image_acquired, nullptr);
    if (sc.render_done) vkDestroySemaphore(ctx.device, sc.render_done, nullptr);
    for (auto f : sc.fences) if (f) vkDestroyFence(ctx.device, f, nullptr);
    if (!sc.cmds.empty())
        vkFreeCommandBuffers(ctx.device, ctx.command_pool,
                             static_cast<uint32_t>(sc.cmds.size()), sc.cmds.data());
    for (auto fb : sc.framebuffers) if (fb) vkDestroyFramebuffer(ctx.device, fb, nullptr);
    if (sc.render_pass) vkDestroyRenderPass(ctx.device, sc.render_pass, nullptr);
    for (auto v : sc.views) if (v) vkDestroyImageView(ctx.device, v, nullptr);
    if (sc.swapchain) vkDestroySwapchainKHR(ctx.device, sc.swapchain, nullptr);
    sc = {};
}

void clear_and_present(const GpuContext& ctx, Swapchain& sc) {
    uint32_t idx = 0;
    VkResult acq = vkAcquireNextImageKHR(ctx.device, sc.swapchain, UINT64_MAX,
                                         sc.image_acquired, VK_NULL_HANDLE, &idx);
    if (acq != VK_SUCCESS && acq != VK_SUBOPTIMAL_KHR) return;
    vkWaitForFences(ctx.device, 1, &sc.fences[idx], VK_TRUE, UINT64_MAX);
    vkResetFences(ctx.device, 1, &sc.fences[idx]);
    VkCommandBuffer cmd = sc.cmds[idx];
    vkResetCommandBuffer(cmd, 0);
    VkCommandBufferBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &begin);
    VkClearValue clear{};
    clear.color = {{0.02f, 0.03f, 0.08f, 1.0f}};
    VkRenderPassBeginInfo rp{};
    rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rp.renderPass = sc.render_pass;
    rp.framebuffer = sc.framebuffers[idx];
    rp.renderArea.extent = sc.extent;
    rp.clearValueCount = 1;
    rp.pClearValues = &clear;
    vkCmdBeginRenderPass(cmd, &rp, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdEndRenderPass(cmd);
    vkEndCommandBuffer(cmd);
    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = &sc.image_acquired;
    submit.pWaitDstStageMask = &wait_stage;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &sc.render_done;
    vkQueueSubmit(ctx.graphics_queue, 1, &submit, sc.fences[idx]);
    VkPresentInfoKHR present{};
    present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores = &sc.render_done;
    present.swapchainCount = 1;
    present.pSwapchains = &sc.swapchain;
    present.pImageIndices = &idx;
    vkQueuePresentKHR(ctx.graphics_queue, &present);
}

RenderPipeline create_point_pipeline(const GpuContext& ctx, VkRenderPass render_pass) {
    RenderPipeline p;
    VkShaderModuleCreateInfo vinfo{};
    vinfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    vinfo.codeSize = sizeof(render_vert_spv);
    vinfo.pCode = reinterpret_cast<const uint32_t*>(render_vert_spv);
    if (vkCreateShaderModule(ctx.device, &vinfo, nullptr, &p.vert) != VK_SUCCESS)
        throw std::runtime_error("failed to create vertex shader module");
    VkShaderModuleCreateInfo finfo{};
    finfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    finfo.codeSize = sizeof(render_frag_spv);
    finfo.pCode = reinterpret_cast<const uint32_t*>(render_frag_spv);
    if (vkCreateShaderModule(ctx.device, &finfo, nullptr, &p.frag) != VK_SUCCESS)
        throw std::runtime_error("failed to create fragment shader module");
    VkDescriptorSetLayoutBinding binding{};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    VkDescriptorSetLayoutCreateInfo layout_info{};
    layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.bindingCount = 1;
    layout_info.pBindings = &binding;
    if (vkCreateDescriptorSetLayout(ctx.device, &layout_info, nullptr, &p.set_layout) != VK_SUCCESS)
        throw std::runtime_error("failed to create render descriptor set layout");
    VkPushConstantRange push_range{};
    push_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    push_range.offset = 0;
    push_range.size = 80;
    VkPipelineLayoutCreateInfo pl_info{};
    pl_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pl_info.setLayoutCount = 1;
    pl_info.pSetLayouts = &p.set_layout;
    pl_info.pushConstantRangeCount = 1;
    pl_info.pPushConstantRanges = &push_range;
    if (vkCreatePipelineLayout(ctx.device, &pl_info, nullptr, &p.pipeline_layout) != VK_SUCCESS)
        throw std::runtime_error("failed to create render pipeline layout");
    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = p.vert;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = p.frag;
    stages[1].pName = "main";
    VkPipelineVertexInputStateCreateInfo vi{};
    vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    VkPipelineInputAssemblyStateCreateInfo ia{};
    ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    VkPipelineViewportStateCreateInfo vp{};
    vp.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vp.viewportCount = 1;
    vp.scissorCount = 1;
    VkPipelineRasterizationStateCreateInfo rs{};
    rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.cullMode = VK_CULL_MODE_NONE;
    rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rs.lineWidth = 1.0f;
    VkPipelineMultisampleStateCreateInfo ms{};
    ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    VkPipelineColorBlendAttachmentState blend_att{};
    blend_att.blendEnable = VK_TRUE;
    blend_att.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    blend_att.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
    blend_att.colorBlendOp = VK_BLEND_OP_ADD;
    blend_att.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blend_att.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blend_att.alphaBlendOp = VK_BLEND_OP_ADD;
    blend_att.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                               VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo cb{};
    cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cb.attachmentCount = 1;
    cb.pAttachments = &blend_att;
    VkDynamicState dyn_states[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dyn{};
    dyn.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dyn.dynamicStateCount = 2;
    dyn.pDynamicStates = dyn_states;
    VkGraphicsPipelineCreateInfo gp{};
    gp.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    gp.stageCount = 2;
    gp.pStages = stages;
    gp.pVertexInputState = &vi;
    gp.pInputAssemblyState = &ia;
    gp.pViewportState = &vp;
    gp.pRasterizationState = &rs;
    gp.pMultisampleState = &ms;
    gp.pColorBlendState = &cb;
    gp.pDynamicState = &dyn;
    gp.layout = p.pipeline_layout;
    gp.renderPass = render_pass;
    gp.subpass = 0;
    if (vkCreateGraphicsPipelines(ctx.device, VK_NULL_HANDLE, 1, &gp, nullptr, &p.pipeline) != VK_SUCCESS)
        throw std::runtime_error("failed to create graphics pipeline");
    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.maxSets = 1;
    VkDescriptorPoolSize pool_size{};
    pool_size.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    pool_size.descriptorCount = 1;
    pool_info.poolSizeCount = 1;
    pool_info.pPoolSizes = &pool_size;
    if (vkCreateDescriptorPool(ctx.device, &pool_info, nullptr, &p.descriptor_pool) != VK_SUCCESS)
        throw std::runtime_error("failed to create render descriptor pool");
    VkDescriptorSetAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = p.descriptor_pool;
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts = &p.set_layout;
    if (vkAllocateDescriptorSets(ctx.device, &alloc_info, &p.descriptor_set) != VK_SUCCESS)
        throw std::runtime_error("failed to allocate render descriptor set");
    return p;
}

void destroy_render_pipeline(const GpuContext& ctx, RenderPipeline& p) {
    if (p.descriptor_pool) vkDestroyDescriptorPool(ctx.device, p.descriptor_pool, nullptr);
    if (p.pipeline) vkDestroyPipeline(ctx.device, p.pipeline, nullptr);
    if (p.pipeline_layout) vkDestroyPipelineLayout(ctx.device, p.pipeline_layout, nullptr);
    if (p.set_layout) vkDestroyDescriptorSetLayout(ctx.device, p.set_layout, nullptr);
    if (p.vert) vkDestroyShaderModule(ctx.device, p.vert, nullptr);
    if (p.frag) vkDestroyShaderModule(ctx.device, p.frag, nullptr);
    p = {};
}

void bind_render_buffer(const GpuContext& ctx, RenderPipeline& p, const Buffer& buf,
                        VkDeviceSize offset, VkDeviceSize range) {
    VkDescriptorBufferInfo buf_info{};
    buf_info.buffer = buf.buffer;
    buf_info.offset = offset;
    buf_info.range = range;
    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = p.descriptor_set;
    write.dstBinding = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    write.pBufferInfo = &buf_info;
    vkUpdateDescriptorSets(ctx.device, 1, &write, 0, nullptr);
}

struct RenderPush {
    uint32_t entity_count;
    float time;
    alignas(16) float vp[16];
};
static_assert(sizeof(RenderPush) == 80, "RenderPush must match GLSL push constant block");

void render_frame(const GpuContext& ctx, Swapchain& sc,
                  ComputePipeline& cp, RenderPipeline& rp, DispatchRunner& runner,
                  const Buffer& buf,
                  uint32_t chunk_count, uint32_t entity_count, float dt,
                  const float* vp_data) {
    uint32_t idx = 0;
    VkResult acq = vkAcquireNextImageKHR(ctx.device, sc.swapchain, UINT64_MAX,
                                         sc.image_acquired, VK_NULL_HANDLE, &idx);
    if (acq != VK_SUCCESS && acq != VK_SUBOPTIMAL_KHR) return;

    vkWaitForFences(ctx.device, 1, &sc.fences[idx], VK_TRUE, UINT64_MAX);
    vkResetFences(ctx.device, 1, &sc.fences[idx]);

    auto fence = runner_dispatch(ctx, runner, cp, buf, chunk_count, dt, 0,
                                 static_cast<VkDeviceSize>(chunk_count) * CHUNK_SIZE);

    VkCommandBuffer cmd = sc.cmds[idx];
    vkResetCommandBuffer(cmd, 0);
    VkCommandBufferBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &begin);

    vkWaitForFences(ctx.device, 1, &fence, VK_TRUE, UINT64_MAX);

    VkClearValue clear{};
    clear.color = {{0.01f, 0.02f, 0.05f, 1.0f}};
    VkRenderPassBeginInfo rp_begin{};
    rp_begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rp_begin.renderPass = sc.render_pass;
    rp_begin.framebuffer = sc.framebuffers[idx];
    rp_begin.renderArea.extent = sc.extent;
    rp_begin.clearValueCount = 1;
    rp_begin.pClearValues = &clear;
    vkCmdBeginRenderPass(cmd, &rp_begin, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport{};
    viewport.width = static_cast<float>(sc.extent.width);
    viewport.height = static_cast<float>(sc.extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.extent = sc.extent;
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, rp.pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            rp.pipeline_layout, 0, 1, &rp.descriptor_set, 0, nullptr);

    RenderPush render_push{};
    render_push.entity_count = entity_count;
    render_push.time = 0.0f;
    std::memcpy(render_push.vp, vp_data, sizeof(float) * 16);

    vkCmdPushConstants(cmd, rp.pipeline_layout,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(render_push), &render_push);

    vkCmdDraw(cmd, entity_count, 1, 0, 0);

    vkCmdEndRenderPass(cmd);
    vkEndCommandBuffer(cmd);

    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = &sc.image_acquired;
    submit.pWaitDstStageMask = &wait_stage;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &sc.render_done;
    vkQueueSubmit(ctx.graphics_queue, 1, &submit, sc.fences[idx]);

    VkPresentInfoKHR present{};
    present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores = &sc.render_done;
    present.swapchainCount = 1;
    present.pSwapchains = &sc.swapchain;
    present.pImageIndices = &idx;
    vkQueuePresentKHR(ctx.graphics_queue, &present);
}

} // namespace octolite
