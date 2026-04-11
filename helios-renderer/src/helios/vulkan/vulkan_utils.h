#pragma once

#include "helios/rhi/rhi_types.h"
#include <vulkan/vulkan.h>

namespace helios::rhi::vulkan {

inline VkFormat to_vk_format(TextureFormat format)
{
    switch (format) {
        case TextureFormat::R8:              return VK_FORMAT_R8_UNORM;
        case TextureFormat::RG8:             return VK_FORMAT_R8G8_UNORM;
        case TextureFormat::RGBA8:           return VK_FORMAT_R8G8B8A8_UNORM;
        case TextureFormat::BGRA8:           return VK_FORMAT_B8G8R8A8_UNORM;
        case TextureFormat::RG16F:           return VK_FORMAT_R16G16_SFLOAT;
        case TextureFormat::RGBA16F:         return VK_FORMAT_R16G16B16A16_SFLOAT;
        case TextureFormat::R32F:            return VK_FORMAT_R32_SFLOAT;
        case TextureFormat::RG32F:           return VK_FORMAT_R32G32_SFLOAT;
        case TextureFormat::RGB32F:          return VK_FORMAT_R32G32B32_SFLOAT;
        case TextureFormat::RGBA32F:         return VK_FORMAT_R32G32B32A32_SFLOAT;
        case TextureFormat::Depth32F:        return VK_FORMAT_D32_SFLOAT;
        case TextureFormat::Depth24Stencil8: return VK_FORMAT_D24_UNORM_S8_UINT;
    }
    return VK_FORMAT_UNDEFINED;
}

inline TextureFormat from_vk_format(VkFormat format)
{
    switch (format) {
        case VK_FORMAT_R8_UNORM:                  return TextureFormat::R8;
        case VK_FORMAT_R8G8_UNORM:                return TextureFormat::RG8;
        case VK_FORMAT_R8G8B8A8_UNORM:            return TextureFormat::RGBA8;
        case VK_FORMAT_B8G8R8A8_UNORM:            return TextureFormat::BGRA8;
        case VK_FORMAT_R16G16_SFLOAT:             return TextureFormat::RG16F;
        case VK_FORMAT_R16G16B16A16_SFLOAT:       return TextureFormat::RGBA16F;
        case VK_FORMAT_R32_SFLOAT:                return TextureFormat::R32F;
        case VK_FORMAT_R32G32_SFLOAT:             return TextureFormat::RG32F;
        case VK_FORMAT_R32G32B32_SFLOAT:          return TextureFormat::RGB32F;
        case VK_FORMAT_R32G32B32A32_SFLOAT:       return TextureFormat::RGBA32F;
        case VK_FORMAT_D32_SFLOAT:                return TextureFormat::Depth32F;
        case VK_FORMAT_D24_UNORM_S8_UINT:         return TextureFormat::Depth24Stencil8;
        default:                                  return TextureFormat::RGBA8;
    }
}

inline VkAttachmentLoadOp to_vk_load_op(LoadOp op)
{
    switch (op) {
        case LoadOp::Load:     return VK_ATTACHMENT_LOAD_OP_LOAD;
        case LoadOp::Clear:    return VK_ATTACHMENT_LOAD_OP_CLEAR;
        case LoadOp::DontCare: return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    }
    return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
}

inline VkAttachmentStoreOp to_vk_store_op(StoreOp op)
{
    switch (op) {
        case StoreOp::Store:    return VK_ATTACHMENT_STORE_OP_STORE;
        case StoreOp::DontCare: return VK_ATTACHMENT_STORE_OP_DONT_CARE;
    }
    return VK_ATTACHMENT_STORE_OP_DONT_CARE;
}

inline VkShaderStageFlags to_vk_shader_stage_flags(ShaderStage stage)
{
    VkShaderStageFlags flags = 0;
    if (has_flag(stage, ShaderStage::Vertex))   flags |= VK_SHADER_STAGE_VERTEX_BIT;
    if (has_flag(stage, ShaderStage::Fragment))  flags |= VK_SHADER_STAGE_FRAGMENT_BIT;
    if (has_flag(stage, ShaderStage::Compute))   flags |= VK_SHADER_STAGE_COMPUTE_BIT;
    if (has_flag(stage, ShaderStage::Geometry))  flags |= VK_SHADER_STAGE_GEOMETRY_BIT;
    return flags;
}

inline VkPipelineStageFlags2 to_vk_pipeline_stage2(ShaderStage stage)
{
    VkPipelineStageFlags2 flags = 0;
    if (has_flag(stage, ShaderStage::Vertex))   flags |= VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
    if (has_flag(stage, ShaderStage::Fragment))  flags |= VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    if (has_flag(stage, ShaderStage::Compute))   flags |= VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    if (has_flag(stage, ShaderStage::Geometry))  flags |= VK_PIPELINE_STAGE_2_GEOMETRY_SHADER_BIT;
    return flags;
}

inline VkCullModeFlags to_vk_cull_mode(CullMode mode)
{
    switch (mode) {
        case CullMode::None:  return VK_CULL_MODE_NONE;
        case CullMode::Front: return VK_CULL_MODE_FRONT_BIT;
        case CullMode::Back:  return VK_CULL_MODE_BACK_BIT;
    }
    return VK_CULL_MODE_NONE;
}

inline VkCompareOp to_vk_compare_op(DepthCompare compare)
{
    switch (compare) {
        case DepthCompare::Less:         return VK_COMPARE_OP_LESS;
        case DepthCompare::LessEqual:    return VK_COMPARE_OP_LESS_OR_EQUAL;
        case DepthCompare::Greater:      return VK_COMPARE_OP_GREATER;
        case DepthCompare::GreaterEqual: return VK_COMPARE_OP_GREATER_OR_EQUAL;
        case DepthCompare::Never:        return VK_COMPARE_OP_NEVER;
        case DepthCompare::Always:       return VK_COMPARE_OP_ALWAYS;
    }
    return VK_COMPARE_OP_LESS;
}

inline VkPresentModeKHR to_vk_present_mode(PresentMode mode)
{
    switch (mode) {
        case PresentMode::Immediate: return VK_PRESENT_MODE_IMMEDIATE_KHR;
        case PresentMode::Fifo:      return VK_PRESENT_MODE_FIFO_KHR;
        case PresentMode::Mailbox:   return VK_PRESENT_MODE_MAILBOX_KHR;
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

inline VkSamplerAddressMode to_vk_sampler_mode(SamplerMode mode)
{
    switch (mode) {
        case SamplerMode::Repeat:         return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        case SamplerMode::ClampToEdge:    return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        case SamplerMode::MirroredRepeat: return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    }
    return VK_SAMPLER_ADDRESS_MODE_REPEAT;
}

inline VkDescriptorType to_vk_descriptor_type(DescriptorType type)
{
    switch (type) {
        case DescriptorType::UniformBuffer:        return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        case DescriptorType::StorageBuffer:        return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        case DescriptorType::CombinedImageSampler: return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        case DescriptorType::StorageImage:         return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    }
    return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
}

inline bool is_depth_format(TextureFormat format)
{
    return format == TextureFormat::Depth32F || format == TextureFormat::Depth24Stencil8;
}

inline uint32_t format_bytes_per_pixel(TextureFormat format)
{
    switch (format) {
        case TextureFormat::R8:              return 1;
        case TextureFormat::RG8:             return 2;
        case TextureFormat::RGBA8:           return 4;
        case TextureFormat::BGRA8:           return 4;
        case TextureFormat::RG16F:           return 4;
        case TextureFormat::RGBA16F:         return 8;
        case TextureFormat::R32F:            return 4;
        case TextureFormat::RG32F:           return 8;
        case TextureFormat::RGB32F:          return 12;
        case TextureFormat::RGBA32F:         return 16;
        case TextureFormat::Depth32F:        return 4;
        case TextureFormat::Depth24Stencil8: return 4;
    }
    return 4;
}

inline VkImageViewType to_vk_view_type(TextureType type)
{
    switch (type) {
        case TextureType::Texture2D:      return VK_IMAGE_VIEW_TYPE_2D;
        case TextureType::TextureCube:    return VK_IMAGE_VIEW_TYPE_CUBE;
        case TextureType::Texture2DArray: return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    }
    return VK_IMAGE_VIEW_TYPE_2D;
}

} // namespace helios::rhi::vulkan
