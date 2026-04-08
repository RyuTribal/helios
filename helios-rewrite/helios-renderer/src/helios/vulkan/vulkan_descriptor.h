#pragma once

#include "helios/rhi/rhi_descriptor.h"
#include "helios/rhi/rhi_types.h"
#include <vulkan/vulkan.h>

namespace helios::rhi::vulkan {

class VulkanDevice;

// RAII wrapper around VkDescriptorSetLayout.
class VulkanDescriptorSetLayout : public rhi::DescriptorSetLayout {
public:
    VulkanDescriptorSetLayout() = default;
    VulkanDescriptorSetLayout(VulkanDevice& device, const DescriptorSetLayoutDesc& desc);
    ~VulkanDescriptorSetLayout() override;

    VulkanDescriptorSetLayout(VulkanDescriptorSetLayout&& other) noexcept;
    VulkanDescriptorSetLayout& operator=(VulkanDescriptorSetLayout&& other) noexcept;
    VulkanDescriptorSetLayout(const VulkanDescriptorSetLayout&) = delete;
    VulkanDescriptorSetLayout& operator=(const VulkanDescriptorSetLayout&) = delete;

    VkDescriptorSetLayout vk_layout() const { return m_layout; }
    explicit operator bool() const { return m_layout != VK_NULL_HANDLE; }

private:
    void destroy();

    VkDescriptorSetLayout m_layout = VK_NULL_HANDLE;
    VulkanDevice* m_device         = nullptr;
};

// RAII wrapper around VkDescriptorSet.
// Allocated from the device's descriptor pool.
// Freed when the pool is reset (or explicitly via vkFreeDescriptorSets
// since the pool uses VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT).
class VulkanDescriptorSet : public rhi::DescriptorSet {
public:
    VulkanDescriptorSet() = default;
    VulkanDescriptorSet(VulkanDevice& device, VkDescriptorSet set);
    ~VulkanDescriptorSet() override;

    VulkanDescriptorSet(VulkanDescriptorSet&& other) noexcept;
    VulkanDescriptorSet& operator=(VulkanDescriptorSet&& other) noexcept;
    VulkanDescriptorSet(const VulkanDescriptorSet&) = delete;
    VulkanDescriptorSet& operator=(const VulkanDescriptorSet&) = delete;

    VkDescriptorSet vk_set() const { return m_set; }
    explicit operator bool() const { return m_set != VK_NULL_HANDLE; }

private:
    void destroy();

    VkDescriptorSet m_set  = VK_NULL_HANDLE;
    VulkanDevice* m_device = nullptr;
};

} // namespace helios::rhi::vulkan
