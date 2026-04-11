#include "helios/vulkan/vulkan_command_buffer.h"
#include "helios/vulkan/vulkan_device.h"
#include "helios/vulkan/vulkan_context.h"
#include "helios/vulkan/vulkan_buffer.h"
#include "helios/vulkan/vulkan_pipeline.h"
#include "helios/vulkan/vulkan_render_pass.h"
#include "helios/vulkan/vulkan_framebuffer.h"
#include "helios/vulkan/vulkan_descriptor.h"
#include "helios/vulkan/vulkan_texture.h"
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

void VulkanCommandBuffer::begin_render_pass(const rhi::RenderPass& render_pass,
                                            const rhi::Framebuffer& framebuffer,
                                            const ClearValues& clear)
{
    HELIOS_ASSERT(m_command_buffer != VK_NULL_HANDLE, "Command buffer not initialized");

    const auto& vk_rp = static_cast<const VulkanRenderPass&>(render_pass);
    const auto& vk_fb = static_cast<const VulkanFramebuffer&>(framebuffer);

    const auto& views = vk_fb.attachment_views();
    const auto& rp_desc = vk_rp.desc();

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
    rendering_info.renderArea           = {{0, 0}, {vk_fb.width(), vk_fb.height()}};
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

void VulkanCommandBuffer::bind_pipeline(const rhi::Pipeline& pipeline)
{
    HELIOS_ASSERT(m_command_buffer != VK_NULL_HANDLE, "Command buffer not initialized");

    const auto& vk_pipeline = static_cast<const VulkanPipeline&>(pipeline);
    HELIOS_ASSERT(vk_pipeline, "Pipeline must be valid");

    vkCmdBindPipeline(m_command_buffer, vk_pipeline.bind_point(), vk_pipeline.vk_pipeline());
    m_current_pipeline_layout = vk_pipeline.vk_layout();
    m_current_bind_point      = vk_pipeline.bind_point();
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

void VulkanCommandBuffer::bind_vertex_buffer(const rhi::Buffer& buffer, uint32_t binding)
{
    HELIOS_ASSERT(m_command_buffer != VK_NULL_HANDLE, "Command buffer not initialized");

    const auto& vk_buf = static_cast<const VulkanBuffer&>(buffer);
    VkBuffer buffers[] = { vk_buf.vk_buffer() };
    VkDeviceSize offsets[] = { 0 };

    vkCmdBindVertexBuffers(m_command_buffer, binding, 1, buffers, offsets);
}

void VulkanCommandBuffer::bind_index_buffer(const rhi::Buffer& buffer, IndexType type)
{
    HELIOS_ASSERT(m_command_buffer != VK_NULL_HANDLE, "Command buffer not initialized");

    const auto& vk_buf = static_cast<const VulkanBuffer&>(buffer);
    VkIndexType vk_type = VK_INDEX_TYPE_UINT32;
    if (type == IndexType::Uint16)
        vk_type = VK_INDEX_TYPE_UINT16;

    vkCmdBindIndexBuffer(m_command_buffer, vk_buf.vk_buffer(), 0, vk_type);
}

void VulkanCommandBuffer::bind_descriptor_set(uint32_t set, const rhi::DescriptorSet& ds)
{
    HELIOS_ASSERT(m_command_buffer != VK_NULL_HANDLE, "Command buffer not initialized");
    HELIOS_ASSERT(m_current_pipeline_layout != VK_NULL_HANDLE, "Must bind a pipeline before descriptor sets");

    const auto& vk_ds = static_cast<const VulkanDescriptorSet&>(ds);
    VkDescriptorSet vk_set = vk_ds.vk_set();
    vkCmdBindDescriptorSets(m_command_buffer, m_current_bind_point,
                            m_current_pipeline_layout, set, 1, &vk_set, 0, nullptr);
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

void VulkanCommandBuffer::transition_image(rhi::Texture& texture,
                                           TextureLayout old_layout,
                                           TextureLayout new_layout)
{
    HELIOS_ASSERT(m_command_buffer != VK_NULL_HANDLE, "Command buffer not initialized");

    auto& vk_tex = static_cast<VulkanTexture&>(texture);

    auto to_vk_layout = [](TextureLayout l) -> VkImageLayout {
        switch (l) {
            case TextureLayout::Undefined:       return VK_IMAGE_LAYOUT_UNDEFINED;
            case TextureLayout::General:          return VK_IMAGE_LAYOUT_GENERAL;
            case TextureLayout::ColorAttachment:  return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            case TextureLayout::DepthAttachment:  return VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
            case TextureLayout::ShaderReadOnly:   return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            case TextureLayout::TransferSrc:      return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            case TextureLayout::TransferDst:      return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            case TextureLayout::PresentSrc:       return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            default:                              return VK_IMAGE_LAYOUT_UNDEFINED;
        }
    };

    VkImageLayout vk_old = to_vk_layout(old_layout);
    VkImageLayout vk_new = to_vk_layout(new_layout);

    // Determine aspect mask from the texture's actual format, not the layout.
    // A depth texture may transition between ShaderReadOnly, TransferDst, etc.
    VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    if (is_depth_format(texture.format())) {
        aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
    }

    VulkanTexture::transition_layout(m_command_buffer, vk_tex.vk_image(),
                                     vk_old, vk_new, aspect);
    vk_tex.set_current_layout(vk_new);
}

// =========================================================================
//  Dynamic rendering (no render pass / framebuffer)
// =========================================================================

void VulkanCommandBuffer::begin_rendering(rhi::Texture* color, rhi::Texture* depth,
                                          const ClearValues& clear,
                                          uint32_t width, uint32_t height) {
    HELIOS_ASSERT(m_command_buffer != VK_NULL_HANDLE, "Command buffer not initialized");

    VkRenderingAttachmentInfo color_att{};
    if (color) {
        auto& vk_color = static_cast<VulkanTexture&>(*color);

        // Workaround: some Intel Mesa drivers discard LOAD_OP_CLEAR results when
        // no draw calls happen in the pass. Do an explicit vkCmdClearColorImage
        // before beginning, then use LOAD_OP_LOAD to preserve it.
        VulkanTexture::transition_layout(m_command_buffer, vk_color.vk_image(),
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        VkClearColorValue vk_clear = {{clear.color[0], clear.color[1],
                                        clear.color[2], clear.color[3]}};
        VkImageSubresourceRange range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        vkCmdClearColorImage(m_command_buffer, vk_color.vk_image(),
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &vk_clear, 1, &range);

        VulkanTexture::transition_layout(m_command_buffer, vk_color.vk_image(),
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        vk_color.set_current_layout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        color_att.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        color_att.imageView = vk_color.vk_image_view();
        color_att.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        color_att.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        color_att.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    }

    VkRenderingAttachmentInfo depth_att{};
    if (depth) {
        auto& vk_depth = static_cast<VulkanTexture&>(*depth);
        VulkanTexture::transition_layout(m_command_buffer, vk_depth.vk_image(),
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
            VK_IMAGE_ASPECT_DEPTH_BIT);
        vk_depth.set_current_layout(VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

        depth_att.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        depth_att.imageView = vk_depth.vk_image_view();
        depth_att.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
        depth_att.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depth_att.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depth_att.clearValue.depthStencil = {clear.depth, clear.stencil};
    }

    VkRenderingInfo rendering{};
    rendering.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    rendering.renderArea = {{0, 0}, {width, height}};
    rendering.layerCount = 1;
    rendering.colorAttachmentCount = color ? 1u : 0u;
    rendering.pColorAttachments = color ? &color_att : nullptr;
    rendering.pDepthAttachment = depth ? &depth_att : nullptr;

    vkCmdBeginRendering(m_command_buffer, &rendering);
}

void VulkanCommandBuffer::end_rendering() {
    HELIOS_ASSERT(m_command_buffer != VK_NULL_HANDLE, "Command buffer not initialized");
    vkCmdEndRendering(m_command_buffer);
}

// =========================================================================
//  Clear
// =========================================================================

void VulkanCommandBuffer::clear_image(rhi::Texture& texture, float r, float g, float b, float a) {
    HELIOS_ASSERT(m_command_buffer != VK_NULL_HANDLE, "Command buffer not initialized");
    auto& vk_tex = static_cast<VulkanTexture&>(texture);
    VkClearColorValue color = {{r, g, b, a}};
    VkImageSubresourceRange range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCmdClearColorImage(m_command_buffer, vk_tex.vk_image(),
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &color, 1, &range);
}

// =========================================================================
//  Transfer
// =========================================================================

void VulkanCommandBuffer::copy_buffer(const rhi::Buffer& src, const rhi::Buffer& dst,
                                      uint32_t size)
{
    HELIOS_ASSERT(m_command_buffer != VK_NULL_HANDLE, "Command buffer not initialized");

    const auto& vk_src = static_cast<const VulkanBuffer&>(src);
    const auto& vk_dst = static_cast<const VulkanBuffer&>(dst);

    VkBufferCopy copy_region{};
    copy_region.size = size;

    vkCmdCopyBuffer(m_command_buffer, vk_src.vk_buffer(), vk_dst.vk_buffer(), 1, &copy_region);
}

void VulkanCommandBuffer::blit_image(const rhi::Texture& src, const rhi::Texture& dst,
                                     uint32_t src_width, uint32_t src_height,
                                     uint32_t dst_width, uint32_t dst_height) {
    HELIOS_ASSERT(m_command_buffer != VK_NULL_HANDLE, "Command buffer not initialized");

    auto& vk_src = static_cast<const VulkanTexture&>(src);
    auto& vk_dst = static_cast<const VulkanTexture&>(dst);

    VkImageBlit region{};
    region.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.srcOffsets[0] = {0, 0, 0};
    region.srcOffsets[1] = {static_cast<int32_t>(src_width),
                            static_cast<int32_t>(src_height), 1};
    region.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.dstOffsets[0] = {0, 0, 0};
    region.dstOffsets[1] = {static_cast<int32_t>(dst_width),
                            static_cast<int32_t>(dst_height), 1};

    vkCmdBlitImage(m_command_buffer,
        vk_src.vk_image(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        vk_dst.vk_image(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1, &region, VK_FILTER_LINEAR);
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
