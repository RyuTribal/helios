#pragma once

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
class VulkanCommandBuffer {
public:
    VulkanCommandBuffer() = default;
    VulkanCommandBuffer(VulkanDevice& device);
    ~VulkanCommandBuffer();

    VulkanCommandBuffer(VulkanCommandBuffer&& other) noexcept;
    VulkanCommandBuffer& operator=(VulkanCommandBuffer&& other) noexcept;
    VulkanCommandBuffer(const VulkanCommandBuffer&) = delete;
    VulkanCommandBuffer& operator=(const VulkanCommandBuffer&) = delete;

    // Recording
    void begin();
    void end();

    // Render pass (Vulkan 1.3 dynamic rendering)
    void begin_render_pass(const VulkanRenderPass& render_pass,
                           const VulkanFramebuffer& framebuffer,
                           const ClearValues& clear);
    void end_render_pass();

    // Pipeline & state
    void bind_pipeline(const VulkanPipeline& pipeline);
    void set_viewport(float x, float y, float width, float height);
    void set_scissor(int32_t x, int32_t y, uint32_t width, uint32_t height);

    // Resources
    void bind_vertex_buffer(const VulkanBuffer& buffer, uint32_t binding = 0);
    void bind_index_buffer(const VulkanBuffer& buffer, IndexType type = IndexType::Uint32);
    void bind_descriptor_set(uint32_t set, const VulkanDescriptorSet& descriptor_set);
    void push_constants(ShaderStage stage, uint32_t offset, uint32_t size, const void* data);

    // Draw
    void draw(uint32_t vertex_count, uint32_t instance_count = 1,
              uint32_t first_vertex = 0);
    void draw_indexed(uint32_t index_count, uint32_t instance_count = 1,
                      uint32_t first_index = 0);

    // Compute
    void dispatch(uint32_t x, uint32_t y, uint32_t z);

    // Synchronization (Vulkan 1.3 synchronization2)
    void pipeline_barrier(const BarrierDesc& barrier);

    // Transfer
    void copy_buffer(const VulkanBuffer& src, const VulkanBuffer& dst, uint32_t size);

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
