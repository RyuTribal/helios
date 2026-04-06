#include "pch.h"
#include "Renderer/Vulkan/VulkanBuffer.h"
#include "Renderer/Vulkan/VulkanDevice.h"
#include "Renderer/Vulkan/VulkanContext.h"

namespace Engine {

    static VkBufferUsageFlags MapBufferUsage(BufferUsage usage)
    {
        VkBufferUsageFlags flags = VK_BUFFER_USAGE_TRANSFER_DST_BIT; // Always allow uploads

        if (usage & BufferUsage::Vertex)
            flags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        if (usage & BufferUsage::Index)
            flags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        if (usage & BufferUsage::Uniform)
            flags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        if (usage & BufferUsage::Storage)
            flags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        if (usage & BufferUsage::Transfer)
            flags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

        return flags;
    }

    static VmaMemoryUsage MapMemoryAccess(MemoryAccess access)
    {
        switch (access) {
            case MemoryAccess::GPU_Only:   return VMA_MEMORY_USAGE_GPU_ONLY;
            case MemoryAccess::CPU_to_GPU: return VMA_MEMORY_USAGE_CPU_TO_GPU;
            case MemoryAccess::GPU_to_CPU: return VMA_MEMORY_USAGE_GPU_TO_CPU;
        }
        return VMA_MEMORY_USAGE_GPU_ONLY;
    }

    VulkanBuffer::VulkanBuffer(VulkanDevice* device, const BufferDesc& desc, const void* initialData)
        : m_Device(device), m_Desc(desc)
    {
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = desc.Size;
        bufferInfo.usage = MapBufferUsage(desc.Usage);
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = MapMemoryAccess(desc.Access);

        VkResult result = vmaCreateBuffer(device->GetAllocator(), &bufferInfo, &allocInfo,
                                          &m_Buffer, &m_Allocation, &m_AllocInfo);
        if (result != VK_SUCCESS) {
            HVE_CORE_ERROR_TAG("Vulkan", "Failed to create buffer '{}' (size={})",
                               desc.DebugName, desc.Size);
            return;
        }

        // Upload initial data
        if (initialData) {
            if (desc.Access == MemoryAccess::GPU_Only) {
                // Stage through a CPU-visible buffer, then copy via ImmediateSubmit
                VkBufferCreateInfo stagingBufferInfo{};
                stagingBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
                stagingBufferInfo.size = desc.Size;
                stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
                stagingBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

                VmaAllocationCreateInfo stagingAllocInfo{};
                stagingAllocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

                VkBuffer stagingBuffer = VK_NULL_HANDLE;
                VmaAllocation stagingAllocation = VK_NULL_HANDLE;

                vmaCreateBuffer(device->GetAllocator(), &stagingBufferInfo, &stagingAllocInfo,
                                &stagingBuffer, &stagingAllocation, nullptr);

                void* mapped = nullptr;
                vmaMapMemory(device->GetAllocator(), stagingAllocation, &mapped);
                std::memcpy(mapped, initialData, desc.Size);
                vmaUnmapMemory(device->GetAllocator(), stagingAllocation);

                device->ImmediateSubmit([&](VkCommandBuffer cmd) {
                    VkBufferCopy copyRegion{};
                    copyRegion.size = desc.Size;
                    vkCmdCopyBuffer(cmd, stagingBuffer, m_Buffer, 1, &copyRegion);
                });

                vmaDestroyBuffer(device->GetAllocator(), stagingBuffer, stagingAllocation);
            } else {
                // CPU-accessible -- map and copy directly
                void* mapped = nullptr;
                vmaMapMemory(device->GetAllocator(), m_Allocation, &mapped);
                std::memcpy(mapped, initialData, desc.Size);
                vmaUnmapMemory(device->GetAllocator(), m_Allocation);
            }
        }

        // Debug name
        if (!desc.DebugName.empty()) {
            VulkanContext::SetDebugName(device->GetDevice(), VK_OBJECT_TYPE_BUFFER,
                                       reinterpret_cast<uint64_t>(m_Buffer), desc.DebugName.c_str());
        }

        HVE_CORE_INFO_TAG("Vulkan", "Created buffer '{}' (size={})", desc.DebugName, desc.Size);
    }

    VulkanBuffer::~VulkanBuffer()
    {
        if (m_Buffer != VK_NULL_HANDLE) {
            vmaDestroyBuffer(m_Device->GetAllocator(), m_Buffer, m_Allocation);
            m_Buffer = VK_NULL_HANDLE;
            m_Allocation = VK_NULL_HANDLE;
        }
    }

    void VulkanBuffer::SetData(const void* data, uint32_t size, uint32_t offset)
    {
        if (m_Desc.Access == MemoryAccess::GPU_Only) {
            // Stage through a CPU-visible buffer, then copy via ImmediateSubmit
            VkBufferCreateInfo stagingBufferInfo{};
            stagingBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            stagingBufferInfo.size = size;
            stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            stagingBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            VmaAllocationCreateInfo stagingAllocInfo{};
            stagingAllocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

            VkBuffer stagingBuffer = VK_NULL_HANDLE;
            VmaAllocation stagingAllocation = VK_NULL_HANDLE;

            vmaCreateBuffer(m_Device->GetAllocator(), &stagingBufferInfo, &stagingAllocInfo,
                            &stagingBuffer, &stagingAllocation, nullptr);

            void* mapped = nullptr;
            vmaMapMemory(m_Device->GetAllocator(), stagingAllocation, &mapped);
            std::memcpy(mapped, data, size);
            vmaUnmapMemory(m_Device->GetAllocator(), stagingAllocation);

            m_Device->ImmediateSubmit([&](VkCommandBuffer cmd) {
                VkBufferCopy copyRegion{};
                copyRegion.srcOffset = 0;
                copyRegion.dstOffset = offset;
                copyRegion.size = size;
                vkCmdCopyBuffer(cmd, stagingBuffer, m_Buffer, 1, &copyRegion);
            });

            vmaDestroyBuffer(m_Device->GetAllocator(), stagingBuffer, stagingAllocation);
        } else {
            // CPU-accessible -- map and copy directly
            void* mapped = nullptr;
            vmaMapMemory(m_Device->GetAllocator(), m_Allocation, &mapped);
            std::memcpy(static_cast<uint8_t*>(mapped) + offset, data, size);
            vmaUnmapMemory(m_Device->GetAllocator(), m_Allocation);
        }
    }

    void* VulkanBuffer::Map()
    {
        HVE_CORE_ASSERT(m_Desc.Access != MemoryAccess::GPU_Only,
                         "Cannot map a GPU_Only buffer");

        void* mapped = nullptr;
        vmaMapMemory(m_Device->GetAllocator(), m_Allocation, &mapped);
        return mapped;
    }

    void VulkanBuffer::Unmap()
    {
        vmaUnmapMemory(m_Device->GetAllocator(), m_Allocation);
    }

} // namespace Engine
