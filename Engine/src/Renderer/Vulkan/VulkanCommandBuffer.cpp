#include "pch.h"
#include "Renderer/Vulkan/VulkanCommandBuffer.h"
#include "Renderer/Vulkan/VulkanDevice.h"
#include "Renderer/Vulkan/VulkanContext.h"
#include "Renderer/Vulkan/VulkanBuffer.h"

namespace Engine {

    // Map engine ShaderStage flags to VkShaderStageFlags
    static VkShaderStageFlags MapShaderStageFlags(ShaderStage stage)
    {
        VkShaderStageFlags flags = 0;
        if (stage & ShaderStage::Vertex)
            flags |= VK_SHADER_STAGE_VERTEX_BIT;
        if (stage & ShaderStage::Fragment)
            flags |= VK_SHADER_STAGE_FRAGMENT_BIT;
        if (stage & ShaderStage::Compute)
            flags |= VK_SHADER_STAGE_COMPUTE_BIT;
        if (stage & ShaderStage::Geometry)
            flags |= VK_SHADER_STAGE_GEOMETRY_BIT;
        return flags;
    }

    // Map engine ShaderStage flags to VkPipelineStageFlags2 (synchronization2)
    static VkPipelineStageFlags2 MapPipelineStage2(ShaderStage stage)
    {
        VkPipelineStageFlags2 flags = 0;
        if (stage & ShaderStage::Vertex)
            flags |= VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
        if (stage & ShaderStage::Fragment)
            flags |= VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
        if (stage & ShaderStage::Compute)
            flags |= VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
        if (stage & ShaderStage::Geometry)
            flags |= VK_PIPELINE_STAGE_2_GEOMETRY_SHADER_BIT;
        return flags;
    }

    VulkanCommandBuffer::VulkanCommandBuffer(VulkanDevice* device)
        : m_Device(device)
    {
        // Create a dedicated command pool for this command buffer
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = device->GetGraphicsQueueFamily();

        VkResult result = vkCreateCommandPool(device->GetDevice(), &poolInfo, nullptr, &m_CommandPool);
        if (result != VK_SUCCESS) {
            HVE_CORE_ERROR_TAG("Vulkan", "Failed to create command pool for command buffer");
            return;
        }

        // Allocate the command buffer from the pool
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = m_CommandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;

        result = vkAllocateCommandBuffers(device->GetDevice(), &allocInfo, &m_CommandBuffer);
        if (result != VK_SUCCESS) {
            HVE_CORE_ERROR_TAG("Vulkan", "Failed to allocate command buffer");
            return;
        }

        // Debug names
        VulkanContext::SetDebugName(device->GetDevice(), VK_OBJECT_TYPE_COMMAND_POOL,
                                   reinterpret_cast<uint64_t>(m_CommandPool), "CmdPool");
        VulkanContext::SetDebugName(device->GetDevice(), VK_OBJECT_TYPE_COMMAND_BUFFER,
                                   reinterpret_cast<uint64_t>(m_CommandBuffer), "CmdBuffer");

        HVE_CORE_INFO_TAG("Vulkan", "Created command buffer");
    }

    VulkanCommandBuffer::~VulkanCommandBuffer()
    {
        if (m_CommandPool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(m_Device->GetDevice(), m_CommandPool, nullptr);
            m_CommandPool = VK_NULL_HANDLE;
            m_CommandBuffer = VK_NULL_HANDLE; // Freed with pool
        }
    }

    void VulkanCommandBuffer::Begin()
    {
        vkResetCommandBuffer(m_CommandBuffer, 0);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(m_CommandBuffer, &beginInfo);
    }

    void VulkanCommandBuffer::End()
    {
        vkEndCommandBuffer(m_CommandBuffer);
    }

    void VulkanCommandBuffer::BeginRenderPass(RHIRenderPass* /*renderPass*/,
                                               RHIFramebuffer* /*framebuffer*/,
                                               const ClearValues& /*clear*/)
    {
        // TODO: Implement with Vulkan 1.3 dynamic rendering (vkCmdBeginRendering)
        // once VulkanFramebuffer is available (Task 6). The renderPass parameter
        // is used for pipeline compatibility only; actual rendering uses
        // VkRenderingInfo with color/depth attachment views from the framebuffer.
    }

    void VulkanCommandBuffer::EndRenderPass()
    {
        // TODO: Call vkCmdEndRendering once BeginRenderPass is implemented (Task 6).
    }

    void VulkanCommandBuffer::BindPipeline(RHIPipeline* pipeline)
    {
        if (!pipeline) return;

        // TODO: Cast to VulkanPipeline and bind once VulkanPipeline is available (Task 6).
        // VulkanPipeline* vkPipeline = static_cast<VulkanPipeline*>(pipeline);
        // vkCmdBindPipeline(m_CommandBuffer, vkPipeline->GetBindPoint(), vkPipeline->GetVkPipeline());
        // m_CurrentPipelineLayout = vkPipeline->GetLayout();
        // m_CurrentBindPoint = vkPipeline->GetBindPoint();
    }

    void VulkanCommandBuffer::SetViewport(float x, float y, float width, float height)
    {
        // Vulkan Y is inverted vs OpenGL; use negative height for correct orientation
        VkViewport viewport{};
        viewport.x = x;
        viewport.y = y + height;
        viewport.width = width;
        viewport.height = -height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        vkCmdSetViewport(m_CommandBuffer, 0, 1, &viewport);
    }

    void VulkanCommandBuffer::SetScissor(int32_t x, int32_t y, uint32_t width, uint32_t height)
    {
        VkRect2D scissor{};
        scissor.offset = { x, y };
        scissor.extent = { width, height };

        vkCmdSetScissor(m_CommandBuffer, 0, 1, &scissor);
    }

    void VulkanCommandBuffer::BindVertexBuffer(RHIBuffer* buffer, uint32_t binding)
    {
        VulkanBuffer* vkBuffer = static_cast<VulkanBuffer*>(buffer);
        VkBuffer buffers[] = { vkBuffer->GetVkBuffer() };
        VkDeviceSize offsets[] = { 0 };

        vkCmdBindVertexBuffers(m_CommandBuffer, binding, 1, buffers, offsets);
    }

    void VulkanCommandBuffer::BindIndexBuffer(RHIBuffer* buffer, IndexType type)
    {
        VulkanBuffer* vkBuffer = static_cast<VulkanBuffer*>(buffer);

        VkIndexType vkType = VK_INDEX_TYPE_UINT32;
        if (type == IndexType::Uint16)
            vkType = VK_INDEX_TYPE_UINT16;

        vkCmdBindIndexBuffer(m_CommandBuffer, vkBuffer->GetVkBuffer(), 0, vkType);
    }

    void VulkanCommandBuffer::BindDescriptorSet(uint32_t set, RHIDescriptorSet* descriptorSet)
    {
        if (!descriptorSet) return;

        // TODO: Cast to VulkanDescriptorSet and bind once available (Task 7).
        // VulkanDescriptorSet* vkSet = static_cast<VulkanDescriptorSet*>(descriptorSet);
        // VkDescriptorSet ds = vkSet->GetVkDescriptorSet();
        // vkCmdBindDescriptorSets(m_CommandBuffer, m_CurrentBindPoint,
        //                         m_CurrentPipelineLayout, set, 1, &ds, 0, nullptr);
    }

    void VulkanCommandBuffer::PushConstants(ShaderStage stage, uint32_t offset, uint32_t size, const void* data)
    {
        VkShaderStageFlags stageFlags = MapShaderStageFlags(stage);
        vkCmdPushConstants(m_CommandBuffer, m_CurrentPipelineLayout, stageFlags, offset, size, data);
    }

    void VulkanCommandBuffer::Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex)
    {
        vkCmdDraw(m_CommandBuffer, vertexCount, instanceCount, firstVertex, 0);
    }

    void VulkanCommandBuffer::DrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex)
    {
        vkCmdDrawIndexed(m_CommandBuffer, indexCount, instanceCount, firstIndex, 0, 0);
    }

    void VulkanCommandBuffer::Dispatch(uint32_t x, uint32_t y, uint32_t z)
    {
        vkCmdDispatch(m_CommandBuffer, x, y, z);
    }

    void VulkanCommandBuffer::PipelineBarrier(const BarrierDesc& barrier)
    {
        // Vulkan 1.3 synchronization2
        VkMemoryBarrier2 memoryBarrier{};
        memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
        memoryBarrier.srcStageMask = MapPipelineStage2(barrier.SrcStage);
        memoryBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
        memoryBarrier.dstStageMask = MapPipelineStage2(barrier.DstStage);
        memoryBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;

        VkDependencyInfo depInfo{};
        depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        depInfo.memoryBarrierCount = 1;
        depInfo.pMemoryBarriers = &memoryBarrier;

        vkCmdPipelineBarrier2(m_CommandBuffer, &depInfo);
    }

    void VulkanCommandBuffer::CopyBuffer(RHIBuffer* src, RHIBuffer* dst, uint32_t size)
    {
        VulkanBuffer* vkSrc = static_cast<VulkanBuffer*>(src);
        VulkanBuffer* vkDst = static_cast<VulkanBuffer*>(dst);

        VkBufferCopy copyRegion{};
        copyRegion.size = size;

        vkCmdCopyBuffer(m_CommandBuffer, vkSrc->GetVkBuffer(), vkDst->GetVkBuffer(), 1, &copyRegion);
    }

} // namespace Engine
