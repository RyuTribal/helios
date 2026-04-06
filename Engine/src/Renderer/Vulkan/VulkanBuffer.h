#pragma once

#include "RHI/RHIResources.h"
#include "RHI/RHITypes.h"

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

namespace Engine {

    class VulkanDevice;

    class VulkanBuffer : public RHIBuffer {
    public:
        VulkanBuffer(VulkanDevice* device, const BufferDesc& desc, const void* initialData);
        ~VulkanBuffer() override;

        void SetData(const void* data, uint32_t size, uint32_t offset = 0) override;
        void* Map() override;
        void Unmap() override;
        uint32_t GetSize() const override { return m_Desc.Size; }

        VkBuffer GetVkBuffer() const { return m_Buffer; }

    private:
        VkBuffer m_Buffer = VK_NULL_HANDLE;
        VmaAllocation m_Allocation = VK_NULL_HANDLE;
        VmaAllocationInfo m_AllocInfo{};
        BufferDesc m_Desc;
        VulkanDevice* m_Device;
    };

} // namespace Engine
