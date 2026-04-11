#pragma once

#include "helios/rhi/rhi_types.h"
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace helios::rhi {

// Forward declarations for abstract types.
class Texture;
class Buffer;
class Shader;
class Pipeline;
class CommandBuffer;
class Swapchain;
class RenderPass;
class Framebuffer;
class DescriptorSetLayout;
class DescriptorSet;

// Abstract device interface.
// The device is the primary factory for all GPU resources.
class Device {
public:
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 3;

    virtual ~Device() = default;

    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;

    // Factory methods -- return heap-allocated RAII objects via unique_ptr
    virtual std::unique_ptr<Texture> create_texture(const TextureDesc& desc,
                                                    const void* data = nullptr) = 0;
    virtual std::unique_ptr<Buffer> create_buffer(const BufferDesc& desc,
                                                  const void* data = nullptr) = 0;
    virtual std::unique_ptr<Shader> create_shader(const ShaderDesc& desc) = 0;
    virtual std::unique_ptr<Pipeline> create_graphics_pipeline(const GraphicsPipelineDesc& desc) = 0;
    virtual std::unique_ptr<Pipeline> create_compute_pipeline(const ComputePipelineDesc& desc) = 0;
    virtual std::unique_ptr<CommandBuffer> create_command_buffer() = 0;
    virtual std::unique_ptr<Swapchain> create_swapchain(const SwapchainDesc& desc) = 0;
    virtual std::unique_ptr<RenderPass> create_render_pass(const RenderPassDesc& desc) = 0;
    virtual std::unique_ptr<Framebuffer> create_framebuffer(const FramebufferDesc& desc) = 0;
    virtual std::unique_ptr<DescriptorSetLayout> create_descriptor_set_layout(
        const DescriptorSetLayoutDesc& desc) = 0;
    virtual std::unique_ptr<DescriptorSet> allocate_descriptor_set(
        const DescriptorSetLayout& layout) = 0;
    virtual void update_descriptor_set(DescriptorSet& set,
                                       const std::vector<DescriptorWrite>& writes) = 0;
    virtual void submit(const CommandBuffer& cmd, const SubmitInfo& info = {}) = 0;
    virtual void wait_idle() = 0;

    /// Queue a GPU resource for deferred deletion. The resource will be destroyed
    /// after MAX_FRAMES_IN_FLIGHT frames, when the GPU is guaranteed to be done.
    /// Call this instead of letting unique_ptr destroy resources immediately.
    /// Works with any unique_ptr<T> — the destructor is type-erased.
    template<typename T>
    void defer_destroy(std::unique_ptr<T> resource) {
        if (!resource) return;
        // Capture the unique_ptr in a type-erased lambda. When the lambda is
        // destroyed (during flush), the unique_ptr destructor runs.
        auto* raw = resource.release();
        // Put in the NEXT frame's slot so the resource survives a full
        // MAX_FRAMES_IN_FLIGHT cycle before being destroyed.
        m_deletion_queues[(m_deletion_frame + 1) % MAX_FRAMES_IN_FLIGHT].push_back(
            [raw]() { delete raw; });
    }

    /// Flush deletions for the current frame. Called by frame_end automatically.
    void flush_deferred_deletions();

    // Surface management for multi-window rendering.
    // native_window is a GLFWwindow* (or platform equivalent).
    // Returns an opaque surface handle (VkSurfaceKHR for Vulkan, cast to void*).
    virtual void* create_surface(void* native_window) = 0;
    virtual void destroy_surface(void* surface) = 0;

    // Submit a command buffer with sync objects from a swapchain frame.
    // This is the convenience path: it wires up the swapchain's sync automatically.
    virtual void submit_for_present(const CommandBuffer& cmd, const Swapchain& swapchain) = 0;

    // Native handle escape hatch.
    // Usage: device->native_handle<VkDevice>()
    template<typename T> T native_handle() const;

protected:
    Device() = default;
    Device(Device&&) noexcept = default;
    Device& operator=(Device&&) noexcept = default;

    // Per-frame deletion queues. Each entry is a type-erased destructor lambda.
    // Resources queued now are destroyed MAX_FRAMES_IN_FLIGHT frames later.
    std::vector<std::function<void()>> m_deletion_queues[MAX_FRAMES_IN_FLIGHT];
    uint32_t m_deletion_frame = 0;
};

} // namespace helios::rhi
