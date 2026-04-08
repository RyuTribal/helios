#pragma once

#include "helios/rhi/rhi_command_buffer.h"
#include "helios/rhi/rhi_types.h"
#include <vulkan/vulkan.h>

namespace helios::rhi::vulkan {

class VulkanDevice;
class VulkanPipeline;
class VulkanBuffer;
class VulkanDescriptorSet;
class VulkanRenderPass;
class VulkanFramebuffer;

// RAII command buffer with its own command pool.
// Destructor destroys the pool (which implicitly frees the buffer).
class VulkanCommandBuffer : public rhi::CommandBuffer {
public:
    VulkanCommandBuffer() = default;
    VulkanCommandBuffer(VulkanDevice& device);
    ~VulkanCommandBuffer() override;

    VulkanCommandBuffer(VulkanCommandBuffer&& other) noexcept;
    VulkanCommandBuffer& operator=(VulkanCommandBuffer&& other) noexcept;
    VulkanCommandBuffer(const VulkanCommandBuffer&) = delete;
    VulkanCommandBuffer& operator=(const VulkanCommandBuffer&) = delete;

    // Recording
    void begin() override;
    void end() override;

    // Render pass (Vulkan 1.3 dynamic rendering)
    void begin_render_pass(const rhi::RenderPass& render_pass,
                           const rhi::Framebuffer& framebuffer,
                           const ClearValues& clear) override;
    void end_render_pass() override;

    // Pipeline & state
    void bind_pipeline(const rhi::Pipeline& pipeline) override;
    void set_viewport(float x, float y, float width, float height) override;
    void set_scissor(int32_t x, int32_t y, uint32_t width, uint32_t height) override;

    // Resources
    void bind_vertex_buffer(const rhi::Buffer& buffer, uint32_t binding = 0) override;
    void bind_index_buffer(const rhi::Buffer& buffer, IndexType type = IndexType::Uint32) override;
    void bind_descriptor_set(uint32_t set, const rhi::DescriptorSet& ds) override;
    void push_constants(ShaderStage stage, uint32_t offset, uint32_t size, const void* data) override;

    // Draw
    void draw(uint32_t vertex_count, uint32_t instance_count = 1,
              uint32_t first_vertex = 0) override;
    void draw_indexed(uint32_t index_count, uint32_t instance_count = 1,
                      uint32_t first_index = 0) override;

    // Compute
    void dispatch(uint32_t x, uint32_t y, uint32_t z) override;

    // Synchronization (Vulkan 1.3 synchronization2)
    void pipeline_barrier(const BarrierDesc& barrier) override;

    // Transfer
    void copy_buffer(const rhi::Buffer& src, const rhi::Buffer& dst, uint32_t size) override;

    VkCommandBuffer vk_command_buffer() const { return m_command_buffer; }

    explicit operator bool() const { return m_command_buffer != VK_NULL_HANDLE; }

private:
    void destroy();

    VkCommandBuffer m_command_buffer            = VK_NULL_HANDLE;
    VkCommandPool m_command_pool                = VK_NULL_HANDLE;
    VulkanDevice* m_device                      = nullptr;

    // Tracked state for push constants and descriptor binding
    VkPipelineLayout m_current_pipeline_layout  = VK_NULL_HANDLE;
    VkPipelineBindPoint m_current_bind_point    = VK_PIPELINE_BIND_POINT_GRAPHICS;
};

} // namespace helios::rhi::vulkan
