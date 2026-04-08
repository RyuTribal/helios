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
};

} // namespace helios::rhi
