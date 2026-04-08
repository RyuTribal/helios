#pragma once

#include "helios/rhi/rhi_types.h"
#include <vulkan/vulkan.h>
#include <string>

namespace helios::rhi::vulkan {

class VulkanDevice;

// RAII wrapper around VkShaderModule.
// Loads SPIR-V bytecode and creates the module in the constructor.
// Destructor destroys the module. Move-only.
class VulkanShader {
public:
    VulkanShader() = default;  // null/empty state
    VulkanShader(VulkanDevice& device, const ShaderDesc& desc);
    ~VulkanShader();

    VulkanShader(VulkanShader&& other) noexcept;
    VulkanShader& operator=(VulkanShader&& other) noexcept;
    VulkanShader(const VulkanShader&) = delete;
    VulkanShader& operator=(const VulkanShader&) = delete;

    VkShaderModule module() const { return m_module; }
    VkShaderStageFlagBits vk_stage() const;
    const char* entry_point() const { return m_entry_point.c_str(); }
    ShaderStage stage() const { return m_stage; }

    explicit operator bool() const { return m_module != VK_NULL_HANDLE; }

private:
    void destroy();

    VkShaderModule m_module    = VK_NULL_HANDLE;
    ShaderStage m_stage        = ShaderStage::Vertex;
    std::string m_entry_point  = "main";
    VulkanDevice* m_device     = nullptr;
};

} // namespace helios::rhi::vulkan
