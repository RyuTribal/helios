#include "helios/vulkan/vulkan_descriptor.h"
#include "helios/vulkan/vulkan_device.h"
#include "helios/vulkan/vulkan_context.h"
#include "helios/vulkan/vulkan_utils.h"
#include "helios/vulkan/renderer_log_channels.h"

#include <helios/core/assert.h>

#include <utility>
#include <vector>

namespace helios::rhi::vulkan {

// =========================================================================
//  VulkanDescriptorSetLayout
// =========================================================================

VulkanDescriptorSetLayout::VulkanDescriptorSetLayout(VulkanDevice& device,
                                                     const DescriptorSetLayoutDesc& desc)
    : m_device(&device)
{
    HELIOS_ASSERT(device, "VulkanDevice must be initialized");

    std::vector<VkDescriptorSetLayoutBinding> bindings;
    bindings.reserve(desc.bindings.size());

    for (const auto& binding : desc.bindings) {
        VkDescriptorSetLayoutBinding vk_binding{};
        vk_binding.binding            = binding.binding;
        vk_binding.descriptorType     = to_vk_descriptor_type(binding.type);
        vk_binding.descriptorCount    = binding.count;
        vk_binding.stageFlags         = to_vk_shader_stage_flags(binding.stage);
        vk_binding.pImmutableSamplers = nullptr;
        bindings.push_back(vk_binding);
    }

    VkDescriptorSetLayoutCreateInfo layout_info{};
    layout_info.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.bindingCount = static_cast<uint32_t>(bindings.size());
    layout_info.pBindings    = bindings.data();

    VkResult result = vkCreateDescriptorSetLayout(device.device(), &layout_info, nullptr, &m_layout);
    if (result != VK_SUCCESS) {
        HELIOS_LOG_ERROR(Renderer, "Failed to create descriptor set layout '{}'", desc.debug_name);
        return;
    }

    if (!desc.debug_name.empty()) {
        device.context().set_debug_name(device.device(), VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT,
            reinterpret_cast<uint64_t>(m_layout), desc.debug_name.c_str());
    }

    HELIOS_LOG_INFO(Renderer, "Created descriptor set layout '{}' ({} bindings)",
                    desc.debug_name, desc.bindings.size());
}

VulkanDescriptorSetLayout::~VulkanDescriptorSetLayout()
{
    destroy();
}

VulkanDescriptorSetLayout::VulkanDescriptorSetLayout(VulkanDescriptorSetLayout&& other) noexcept
    : m_layout(std::exchange(other.m_layout, VK_NULL_HANDLE))
    , m_device(std::exchange(other.m_device, nullptr))
{
}

VulkanDescriptorSetLayout& VulkanDescriptorSetLayout::operator=(VulkanDescriptorSetLayout&& other) noexcept
{
    if (this != &other) {
        destroy();
        m_layout = std::exchange(other.m_layout, VK_NULL_HANDLE);
        m_device = std::exchange(other.m_device, nullptr);
    }
    return *this;
}

void VulkanDescriptorSetLayout::destroy()
{
    if (m_layout != VK_NULL_HANDLE && m_device != nullptr) {
        vkDestroyDescriptorSetLayout(m_device->device(), m_layout, nullptr);
        m_layout = VK_NULL_HANDLE;
    }
}

// =========================================================================
//  VulkanDescriptorSet
// =========================================================================

VulkanDescriptorSet::VulkanDescriptorSet(VulkanDevice& device, VkDescriptorSet set,
                                         VkDescriptorPool owning_pool)
    : m_set(set)
    , m_owning_pool(owning_pool)
    , m_device(&device)
{
}

VulkanDescriptorSet::~VulkanDescriptorSet()
{
    destroy();
}

VulkanDescriptorSet::VulkanDescriptorSet(VulkanDescriptorSet&& other) noexcept
    : m_set(std::exchange(other.m_set, VK_NULL_HANDLE))
    , m_owning_pool(std::exchange(other.m_owning_pool, VK_NULL_HANDLE))
    , m_device(std::exchange(other.m_device, nullptr))
{
}

VulkanDescriptorSet& VulkanDescriptorSet::operator=(VulkanDescriptorSet&& other) noexcept
{
    if (this != &other) {
        destroy();
        m_set         = std::exchange(other.m_set, VK_NULL_HANDLE);
        m_owning_pool = std::exchange(other.m_owning_pool, VK_NULL_HANDLE);
        m_device      = std::exchange(other.m_device, nullptr);
    }
    return *this;
}

void VulkanDescriptorSet::destroy()
{
    if (m_set != VK_NULL_HANDLE && m_device != nullptr && m_owning_pool != VK_NULL_HANDLE) {
        vkFreeDescriptorSets(m_device->device(), m_owning_pool, 1, &m_set);
        m_set = VK_NULL_HANDLE;
        m_owning_pool = VK_NULL_HANDLE;
    }
}

} // namespace helios::rhi::vulkan
