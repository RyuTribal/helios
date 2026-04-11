#include "helios/vulkan/vulkan_shader.h"
#include "helios/vulkan/vulkan_device.h"
#include "helios/vulkan/vulkan_context.h"
#include "helios/vulkan/renderer_log_channels.h"

#include <helios/core/assert.h>

#include <utility>

namespace helios::rhi::vulkan {

VulkanShader::VulkanShader(VulkanDevice& device, const ShaderDesc& desc)
    : m_stage(desc.stage)
    , m_entry_point(desc.entry_point)
    , m_device(&device)
{
    HELIOS_ASSERT(!desc.spirv_code.empty(), "SPIR-V code must not be empty");
    HELIOS_ASSERT(desc.spirv_code.size() % 4 == 0,
                  "SPIR-V code size must be a multiple of 4 bytes");

    VkShaderModuleCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = desc.spirv_code.size();
    create_info.pCode = reinterpret_cast<const uint32_t*>(desc.spirv_code.data());

    VkResult result = vkCreateShaderModule(device.device(), &create_info, nullptr, &m_module);
    if (result != VK_SUCCESS) {
        HELIOS_LOG_ERROR(Renderer, "Failed to create shader module '{}'", desc.debug_name);
        return;
    }

    // Debug name
    if (!desc.debug_name.empty()) {
        device.context().set_debug_name(device.device(), VK_OBJECT_TYPE_SHADER_MODULE,
            reinterpret_cast<uint64_t>(m_module), desc.debug_name.c_str());
    }

    HELIOS_LOG_INFO(Renderer, "Created shader module '{}'", desc.debug_name);
}

VulkanShader::~VulkanShader()
{
    destroy();
}

VulkanShader::VulkanShader(VulkanShader&& other) noexcept
    : m_module(std::exchange(other.m_module, VK_NULL_HANDLE))
    , m_stage(std::exchange(other.m_stage, ShaderStage::Vertex))
    , m_entry_point(std::move(other.m_entry_point))
    , m_device(std::exchange(other.m_device, nullptr))
{
}

VulkanShader& VulkanShader::operator=(VulkanShader&& other) noexcept
{
    if (this != &other) {
        destroy();
        m_module      = std::exchange(other.m_module, VK_NULL_HANDLE);
        m_stage       = std::exchange(other.m_stage, ShaderStage::Vertex);
        m_entry_point = std::move(other.m_entry_point);
        m_device      = std::exchange(other.m_device, nullptr);
    }
    return *this;
}

void VulkanShader::destroy()
{
    if (m_module != VK_NULL_HANDLE && m_device != nullptr) {
        vkDestroyShaderModule(m_device->device(), m_module, nullptr);
        m_module = VK_NULL_HANDLE;
    }
}

VkShaderStageFlagBits VulkanShader::vk_stage() const
{
    switch (m_stage) {
        case ShaderStage::Vertex:   return VK_SHADER_STAGE_VERTEX_BIT;
        case ShaderStage::Fragment: return VK_SHADER_STAGE_FRAGMENT_BIT;
        case ShaderStage::Compute:  return VK_SHADER_STAGE_COMPUTE_BIT;
        case ShaderStage::Geometry: return VK_SHADER_STAGE_GEOMETRY_BIT;
    }
    return VK_SHADER_STAGE_VERTEX_BIT;
}

} // namespace helios::rhi::vulkan
