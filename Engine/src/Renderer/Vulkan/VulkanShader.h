#pragma once

#include "RHI/RHIResources.h"
#include "RHI/RHITypes.h"

#include <vulkan/vulkan.h>
#include <string>

namespace Engine {

    class VulkanDevice;

    class VulkanShader : public RHIShader {
    public:
        VulkanShader(VulkanDevice* device, const ShaderDesc& desc);
        ~VulkanShader() override;

        VkShaderModule GetModule() const { return m_Module; }
        VkShaderStageFlagBits GetVkStage() const;
        const char* GetEntryPoint() const { return m_EntryPoint.c_str(); }

    private:
        VkShaderModule m_Module = VK_NULL_HANDLE;
        ShaderStage m_Stage;
        std::string m_EntryPoint;
        VulkanDevice* m_Device;
    };

} // namespace Engine
