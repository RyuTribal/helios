#include "pch.h"
#include "Renderer/Vulkan/VulkanDescriptor.h"
#include "Renderer/Vulkan/VulkanDevice.h"
#include "Renderer/Vulkan/VulkanContext.h"
#include "Renderer/Vulkan/VulkanUtils.h"

namespace Engine {

    // ---- Helper: map DescriptorType to VkDescriptorType ----
    static VkDescriptorType ToVkDescriptorType(DescriptorType type)
    {
        switch (type) {
            case DescriptorType::UniformBuffer:       return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            case DescriptorType::StorageBuffer:       return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            case DescriptorType::CombinedImageSampler: return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            case DescriptorType::StorageImage:        return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        }
        return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    }

    // =========================================================================
    //  VulkanDescriptorSetLayout
    // =========================================================================

    VulkanDescriptorSetLayout::VulkanDescriptorSetLayout(VulkanDevice* device, const DescriptorSetLayoutDesc& desc)
        : m_Device(device)
    {
        std::vector<VkDescriptorSetLayoutBinding> bindings;
        bindings.reserve(desc.Bindings.size());

        for (const auto& binding : desc.Bindings) {
            VkDescriptorSetLayoutBinding vkBinding{};
            vkBinding.binding            = binding.Binding;
            vkBinding.descriptorType     = ToVkDescriptorType(binding.Type);
            vkBinding.descriptorCount    = binding.Count;
            vkBinding.stageFlags         = ToVkShaderStageFlags(binding.Stage);
            vkBinding.pImmutableSamplers = nullptr;
            bindings.push_back(vkBinding);
        }

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings    = bindings.data();

        VkResult result = vkCreateDescriptorSetLayout(device->GetDevice(), &layoutInfo, nullptr, &m_Layout);
        if (result != VK_SUCCESS) {
            HVE_CORE_ERROR_TAG("Vulkan", "Failed to create descriptor set layout '{}'", desc.DebugName);
            return;
        }

        if (!desc.DebugName.empty()) {
            VulkanContext::SetDebugName(device->GetDevice(), VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT,
                reinterpret_cast<uint64_t>(m_Layout), desc.DebugName.c_str());
        }

        HVE_CORE_INFO_TAG("Vulkan", "Created descriptor set layout '{}' ({} bindings)",
                           desc.DebugName, desc.Bindings.size());
    }

    VulkanDescriptorSetLayout::~VulkanDescriptorSetLayout()
    {
        if (m_Layout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(m_Device->GetDevice(), m_Layout, nullptr);
            m_Layout = VK_NULL_HANDLE;
        }
    }

    // =========================================================================
    //  VulkanDescriptorSet
    // =========================================================================

    VulkanDescriptorSet::VulkanDescriptorSet(VkDescriptorSet set, VulkanDevice* device)
        : m_Set(set), m_Device(device)
    {
    }

} // namespace Engine
