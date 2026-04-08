#include "helios/vulkan/vulkan_command_buffer.h"
#include "helios/vulkan/vulkan_device.h"
#include "helios/vulkan/vulkan_context.h"
#include "helios/vulkan/vulkan_buffer.h"
#include "helios/vulkan/vulkan_pipeline.h"
#include "helios/vulkan/vulkan_render_pass.h"
#include "helios/vulkan/vulkan_framebuffer.h"
#include "helios/vulkan/vulkan_descriptor.h"
#include "helios/vulkan/vulkan_utils.h"
#include "helios/vulkan/renderer_log_channels.h"

#include <helios/core/assert.h>

#include <utility>
#include <vector>

namespace helios::rhi::vulkan {

// =========================================================================
//  Constructor
// =========================================================================

VulkanCommandBuffer::VulkanCommandBuffer(VulkanDevice& device)
    : m_device(&device)
{
    HELIOS_ASSERT(device, "VulkanDevice must be initialized");

    // Create a dedicated command pool for this command buffer
    VkCommandPoolCreateInfo pool_info{};
    pool_info.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool_info.queueFamilyIndex = device.graphics_queue_family();

    VkResult result = vkCreateCommandPool(device.device(), &pool_info, nullptr, &m_command_pool);
    if (result != VK_SUCCESS) {
        HELIOS_LOG_ERROR(Renderer, "Failed to create command pool for command buffer");
        return;
    }

    // Allocate the command buffer from the pool
    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool        = m_command_pool;
    alloc_info.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = 1;

    result = vkAllocateCommandBuffers(device.device(), &alloc_info, &m_command_buffer);
    if (result != VK_SUCCESS) {
        HELIOS_LOG_ERROR(Renderer, "Failed to allocate command buffer");
        return;
    }

    // Debug names
    device.context().set_debug_name(device.device(), VK_OBJECT_TYPE_COMMAND_POOL,
                                    reinterpret_cast<uint64_t>(m_command_pool), "CmdPool");
    device.context().set_debug_name(device.device(), VK_OBJECT_TYPE_COMMAND_BUFFER,
                                    reinterpret_cast<uint64_t>(m_command_buffer), "CmdBuffer");

    HELIOS_LOG_INFO(Renderer, "Created command buffer");
}

// =========================================================================
//  Destructor
// =========================================================================

VulkanCommandBuffer::~VulkanCommandBuffer()
{
    destroy();
}

// =========================================================================
//  Move semantics
// =========================================================================

VulkanCommandBuffer::VulkanCommandBuffer(VulkanCommandBuffer&& other) noexcept
    : m_command_buffer(std::exchange(other.m_command_buffer, VK_NULL_HANDLE))
    , m_command_pool(std::exchange(other.m_command_pool, VK_NULL_HANDLE))
    , m_device(std::exchange(other.m_device, nullptr))
    , m_current_pipeline_layout(std::exchange(other.m_current_pipeline_layout, VK_NULL_HANDLE))
    , m_current_bind_point(std::exchange(other.m_current_bind_point, VK_PIPELINE_BIND_POINT_GRAPHICS))
{
}

VulkanCommandBuffer& VulkanCommandBuffer::operator=(VulkanCommandBuffer&& other) noexcept
{
    if (this != &other) {
        destroy();
        m_command_buffer         = std::exchange(other.m_command_buffer, VK_NULL_HANDLE);
        m_command_pool           = std::exchange(other.m_command_pool, VK_NULL_HANDLE);
        m_device                 = std::exchange(other.m_device, nullptr);
        m_current_pipeline_layout = std::exchange(other.m_current_pipeline_layout, VK_NULL_HANDLE);
        m_current_bind_point     = std::exchange(other.m_current_bind_point, VK_PIPELINE_BIND_POINT_GRAPHICS);
    }
    return *this;
}

// =========================================================================
//  Recording
// =========================================================================

void VulkanCommandBuffer::begin()
{
    HELIOS_ASSERT(m_command_buffer != VK_NULL_HANDLE, "Command buffer not initialized");

    vkResetCommandBuffer(m_command_buffer, 0);

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(m_command_buffer, &begin_info);
}

void VulkanCommandBuffer::end()
{
    HELIOS_ASSERT(m_command_buffer != VK_NULL_HANDLE, "Command buffer not initialized");
    vkEndCommandBuffer(m_command_buffer);
}

// =========================================================================
//  Render pass (Vulkan 1.3 dynamic rendering)
// =========================================================================

void VulkanCommandBuffer::begin_render_pass(const VulkanRenderPass& render_pass,
                                            const VulkanFramebuffer& framebuffer,
                                            const ClearValues& clear)
{
    HELIOS_ASSERT(m_command_buffer != VK_NULL_HANDLE, "Command buffer not initialized");

    const auto& views = framebuffer.attachment_views();
    const auto& rp_desc = render_pass.desc();

    // Build color attachments for dynamic rendering
    std::vector<VkRenderingAttachmentInfo> color_attachments;
    for (size_t i = 0; i < rp_desc.color_attachments.size() && i < views.size(); i++) {
        VkRenderingAttachmentInfo info{};
        info.sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        info.imageView   = views[i];
        info.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        info.loadOp      = to_vk_load_op(rp_desc.color_attachments[i].load);
        info.storeOp     = to_vk_store_op(rp_desc.color_attachments[i].store);
        info.clearValue.color = {{clear.color[0], clear.color[1], clear.color[2], clear.color[3]}};
        color_attachments.push_back(info);
    }

    // Depth attachment if present
    VkRenderingAttachmentInfo depth_attachment{};
    bool has_depth = rp_desc.has_depth && views.size() > rp_desc.color_attachments.size();
    if (has_depth) {
        depth_attachment.sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        depth_attachment.imageView   = views.back();
        depth_attachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depth_attachment.loadOp      = to_vk_load_op(rp_desc.depth_attachment.load);
        depth_attachment.storeOp     = to_vk_store_op(rp_desc.depth_attachment.store);
        depth_attachment.clearValue.depthStencil = {clear.depth, clear.stencil};
    }

    VkRenderingInfo rendering_info{};
    rendering_info.sType                = VK_STRUCTURE_TYPE_RENDERING_INFO;
    rendering_info.renderArea           = {{0, 0}, {framebuffer.width(), framebuffer.height()}};
    rendering_info.layerCount           = 1;
    rendering_info.colorAttachmentCount = static_cast<uint32_t>(color_attachments.size());
    rendering_info.pColorAttachments    = color_attachments.data();
    if (has_depth)
        rendering_info.pDepthAttachment = &depth_attachment;

    vkCmdBeginRendering(m_command_buffer, &rendering_info);
}

void VulkanCommandBuffer::end_render_pass()
{
    HELIOS_ASSERT(m_command_buffer != VK_NULL_HANDLE, "Command buffer not initialized");
    vkCmdEndRendering(m_command_buffer);
}

// =========================================================================
//  Pipeline & state
// =========================================================================

void VulkanCommandBuffer::bind_pipeline(const VulkanPipeline& pipeline)
{
    HELIOS_ASSERT(m_command_buffer != VK_NULL_HANDLE, "Command buffer not initialized");
    HELIOS_ASSERT(pipeline, "Pipeline must be valid");

    vkCmdBindPipeline(m_command_buffer, pipeline.bind_point(), pipeline.vk_pipeline());
    m_current_pipeline_layout = pipeline.vk_layout();
    m_current_bind_point      = pipeline.bind_point();
}

void VulkanCommandBuffer::set_viewport(float x, float y, float width, float height)
{
    HELIOS_ASSERT(m_command_buffer != VK_NULL_HANDLE, "Command buffer not initialized");

    // Vulkan Y is inverted vs OpenGL; use negative height for correct orientation
    VkViewport viewport{};
    viewport.x        = x;
    viewport.y        = y + height;
    viewport.width    = width;
    viewport.height   = -height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    vkCmdSetViewport(m_command_buffer, 0, 1, &viewport);
}

void VulkanCommandBuffer::set_scissor(int32_t x, int32_t y, uint32_t width, uint32_t height)
{
    HELIOS_ASSERT(m_command_buffer != VK_NULL_HANDLE, "Command buffer not initialized");

    VkRect2D scissor{};
    scissor.offset = { x, y };
    scissor.extent = { width, height };

    vkCmdSetScissor(m_command_buffer, 0, 1, &scissor);
}

// =========================================================================
//  Resources
// =========================================================================

void VulkanCommandBuffer::bind_vertex_buffer(const VulkanBuffer& buffer, uint32_t binding)
{
    HELIOS_ASSERT(m_command_buffer != VK_NULL_HANDLE, "Command buffer not initialized");

    VkBuffer buffers[] = { buffer.vk_buffer() };
    VkDeviceSize offsets[] = { 0 };

    vkCmdBindVertexBuffers(m_command_buffer, binding, 1, buffers, offsets);
}

void VulkanCommandBuffer::bind_index_buffer(const VulkanBuffer& buffer, IndexType type)
{
    HELIOS_ASSERT(m_command_buffer != VK_NULL_HANDLE, "Command buffer not initialized");

    VkIndexType vk_type = VK_INDEX_TYPE_UINT32;
    if (type == IndexType::Uint16)
        vk_type = VK_INDEX_TYPE_UINT16;

    vkCmdBindIndexBuffer(m_command_buffer, buffer.vk_buffer(), 0, vk_type);
}

void VulkanCommandBuffer::bind_descriptor_set(uint32_t set, const VulkanDescriptorSet& descriptor_set)
{
    HELIOS_ASSERT(m_command_buffer != VK_NULL_HANDLE, "Command buffer not initialized");
    HELIOS_ASSERT(m_current_pipeline_layout != VK_NULL_HANDLE, "Must bind a pipeline before descriptor sets");

    VkDescriptorSet ds = descriptor_set.vk_set();
    vkCmdBindDescriptorSets(m_command_buffer, m_current_bind_point,
                            m_current_pipeline_layout, set, 1, &ds, 0, nullptr);
}

void VulkanCommandBuffer::push_constants(ShaderStage stage, uint32_t offset,
                                         uint32_t size, const void* data)
{
    HELIOS_ASSERT(m_command_buffer != VK_NULL_HANDLE, "Command buffer not initialized");
    HELIOS_ASSERT(m_current_pipeline_layout != VK_NULL_HANDLE, "Must bind a pipeline before push constants");

    VkShaderStageFlags stage_flags = to_vk_shader_stage_flags(stage);
    vkCmdPushConstants(m_command_buffer, m_current_pipeline_layout, stage_flags, offset, size, data);
}

// =========================================================================
//  Draw
// =========================================================================

void VulkanCommandBuffer::draw(uint32_t vertex_count, uint32_t instance_count,
                               uint32_t first_vertex)
{
    HELIOS_ASSERT(m_command_buffer != VK_NULL_HANDLE, "Command buffer not initialized");
    vkCmdDraw(m_command_buffer, vertex_count, instance_count, first_vertex, 0);
}

void VulkanCommandBuffer::draw_indexed(uint32_t index_count, uint32_t instance_count,
                                       uint32_t first_index)
{
    HELIOS_ASSERT(m_command_buffer != VK_NULL_HANDLE, "Command buffer not initialized");
    vkCmdDrawIndexed(m_command_buffer, index_count, instance_count, first_index, 0, 0);
}

// =========================================================================
//  Compute
// =========================================================================

void VulkanCommandBuffer::dispatch(uint32_t x, uint32_t y, uint32_t z)
{
    HELIOS_ASSERT(m_command_buffer != VK_NULL_HANDLE, "Command buffer not initialized");
    vkCmdDispatch(m_command_buffer, x, y, z);
}

// =========================================================================
//  Synchronization (Vulkan 1.3 synchronization2)
// =========================================================================

void VulkanCommandBuffer::pipeline_barrier(const BarrierDesc& barrier)
{
    HELIOS_ASSERT(m_command_buffer != VK_NULL_HANDLE, "Command buffer not initialized");

    VkMemoryBarrier2 memory_barrier{};
    memory_barrier.sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
    memory_barrier.srcStageMask  = to_vk_pipeline_stage2(barrier.src_stage);
    memory_barrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
    memory_barrier.dstStageMask  = to_vk_pipeline_stage2(barrier.dst_stage);
    memory_barrier.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;

    VkDependencyInfo dep_info{};
    dep_info.sType              = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dep_info.memoryBarrierCount = 1;
    dep_info.pMemoryBarriers    = &memory_barrier;

    vkCmdPipelineBarrier2(m_command_buffer, &dep_info);
}

// =========================================================================
//  Transfer
// =========================================================================

void VulkanCommandBuffer::copy_buffer(const VulkanBuffer& src, const VulkanBuffer& dst,
                                      uint32_t size)
{
    HELIOS_ASSERT(m_command_buffer != VK_NULL_HANDLE, "Command buffer not initialized");

    VkBufferCopy copy_region{};
    copy_region.size = size;

    vkCmdCopyBuffer(m_command_buffer, src.vk_buffer(), dst.vk_buffer(), 1, &copy_region);
}

// =========================================================================
//  Private
// =========================================================================

void VulkanCommandBuffer::destroy()
{
    if (m_command_pool != VK_NULL_HANDLE && m_device != nullptr) {
        vkDestroyCommandPool(m_device->device(), m_command_pool, nullptr);
        m_command_pool   = VK_NULL_HANDLE;
        m_command_buffer = VK_NULL_HANDLE; // Freed with pool
    }
}

} // namespace helios::rhi::vulkan
