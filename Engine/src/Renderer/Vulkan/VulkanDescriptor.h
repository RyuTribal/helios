#pragma once

#include "RHI/RHIDescriptor.h"
#include "RHI/RHITypes.h"

#include <vulkan/vulkan.h>

namespace Engine {

    class VulkanDevice;

    class VulkanDescriptorSetLayout : public RHIDescriptorSetLayout {
    public:
        VulkanDescriptorSetLayout(VulkanDevice* device, const DescriptorSetLayoutDesc& desc);
        ~VulkanDescriptorSetLayout() override;

        VkDescriptorSetLayout GetVkLayout() const { return m_Layout; }

    private:
        VkDescriptorSetLayout m_Layout = VK_NULL_HANDLE;
        VulkanDevice* m_Device;
    };

    class VulkanDescriptorSet : public RHIDescriptorSet {
    public:
        VulkanDescriptorSet(VkDescriptorSet set, VulkanDevice* device);
        ~VulkanDescriptorSet() override = default; // Pool manages lifetime

        VkDescriptorSet GetVkSet() const { return m_Set; }

    private:
        VkDescriptorSet m_Set = VK_NULL_HANDLE;
        VulkanDevice* m_Device;
    };

} // namespace Engine
