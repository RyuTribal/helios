#pragma once

#include "RHI/RHITypes.h"
#include <vulkan/vulkan.h>

namespace Engine {

    inline VkFormat ToVkFormat(ImageFormat format)
    {
        switch (format) {
            case ImageFormat::R8:                return VK_FORMAT_R8_UNORM;
            case ImageFormat::RG8:               return VK_FORMAT_R8G8_UNORM;
            case ImageFormat::RGB8:              return VK_FORMAT_R8G8B8_UNORM;
            case ImageFormat::RGBA8:             return VK_FORMAT_R8G8B8A8_UNORM;
            case ImageFormat::RG16F:             return VK_FORMAT_R16G16_SFLOAT;
            case ImageFormat::RGBA16F:           return VK_FORMAT_R16G16B16A16_SFLOAT;
            case ImageFormat::RGBA32F:           return VK_FORMAT_R32G32B32A32_SFLOAT;
            case ImageFormat::R32F:              return VK_FORMAT_R32_SFLOAT;
            case ImageFormat::RG32F:             return VK_FORMAT_R32G32_SFLOAT;
            case ImageFormat::RGB32F:            return VK_FORMAT_R32G32B32_SFLOAT;
            case ImageFormat::DEPTH24_STENCIL8:  return VK_FORMAT_D24_UNORM_S8_UINT;
            case ImageFormat::DEPTH32F:          return VK_FORMAT_D32_SFLOAT;
        }
        return VK_FORMAT_UNDEFINED;
    }

    inline VkAttachmentLoadOp ToVkLoadOp(LoadOp op)
    {
        switch (op) {
            case LoadOp::Load:     return VK_ATTACHMENT_LOAD_OP_LOAD;
            case LoadOp::Clear:    return VK_ATTACHMENT_LOAD_OP_CLEAR;
            case LoadOp::DontCare: return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        }
        return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    }

    inline VkAttachmentStoreOp ToVkStoreOp(StoreOp op)
    {
        switch (op) {
            case StoreOp::Store:    return VK_ATTACHMENT_STORE_OP_STORE;
            case StoreOp::DontCare: return VK_ATTACHMENT_STORE_OP_DONT_CARE;
        }
        return VK_ATTACHMENT_STORE_OP_DONT_CARE;
    }

    inline VkShaderStageFlags ToVkShaderStageFlags(ShaderStage stage)
    {
        VkShaderStageFlags flags = 0;
        if (stage & ShaderStage::Vertex)   flags |= VK_SHADER_STAGE_VERTEX_BIT;
        if (stage & ShaderStage::Fragment) flags |= VK_SHADER_STAGE_FRAGMENT_BIT;
        if (stage & ShaderStage::Compute)  flags |= VK_SHADER_STAGE_COMPUTE_BIT;
        if (stage & ShaderStage::Geometry) flags |= VK_SHADER_STAGE_GEOMETRY_BIT;
        return flags;
    }

} // namespace Engine
