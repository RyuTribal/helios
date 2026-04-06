#include "pch.h"
#include "Renderer/Vulkan/VulkanShader.h"
#include "Renderer/Vulkan/VulkanDevice.h"
#include "Renderer/Vulkan/VulkanContext.h"

namespace Engine {

    VulkanShader::VulkanShader(VulkanDevice* device, const ShaderDesc& desc)
        : m_Device(device), m_Stage(desc.Stage), m_EntryPoint(desc.EntryPoint)
    {
        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = desc.SpirVCode.size();
        createInfo.pCode = reinterpret_cast<const uint32_t*>(desc.SpirVCode.data());

        VkResult result = vkCreateShaderModule(device->GetDevice(), &createInfo, nullptr, &m_Module);
        if (result != VK_SUCCESS) {
            HVE_CORE_ERROR_TAG("Vulkan", "Failed to create shader module '{}'", desc.DebugName);
            return;
        }

        // Debug name
        if (!desc.DebugName.empty()) {
            VulkanContext::SetDebugName(device->GetDevice(), VK_OBJECT_TYPE_SHADER_MODULE,
                                       reinterpret_cast<uint64_t>(m_Module), desc.DebugName.c_str());
        }

        HVE_CORE_INFO_TAG("Vulkan", "Created shader module '{}'", desc.DebugName);
    }

    VulkanShader::~VulkanShader()
    {
        if (m_Module != VK_NULL_HANDLE) {
            vkDestroyShaderModule(m_Device->GetDevice(), m_Module, nullptr);
            m_Module = VK_NULL_HANDLE;
        }
    }

    VkShaderStageFlagBits VulkanShader::GetVkStage() const
    {
        switch (m_Stage) {
            case ShaderStage::Vertex:   return VK_SHADER_STAGE_VERTEX_BIT;
            case ShaderStage::Fragment: return VK_SHADER_STAGE_FRAGMENT_BIT;
            case ShaderStage::Compute:  return VK_SHADER_STAGE_COMPUTE_BIT;
            case ShaderStage::Geometry: return VK_SHADER_STAGE_GEOMETRY_BIT;
        }
        return VK_SHADER_STAGE_VERTEX_BIT;
    }

} // namespace Engine
