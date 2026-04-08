#pragma once

// Compile-time backend selection.
// Only one backend is compiled at a time. The using-declarations below
// make backend types available as helios::rhi::Device, helios::rhi::Texture, etc.
// No virtual dispatch overhead -- callers use concrete types directly.

#include "helios/rhi/rhi_types.h"

#if defined(HELIOS_BACKEND_VULKAN)

    // Forward-declare Vulkan backend types.
    // Full definitions live in helios/vulkan/ headers; include those where needed.
    namespace helios::rhi::vulkan {
        class VulkanContext;
        class VulkanDevice;
        class VulkanTexture;
        class VulkanBuffer;
        class VulkanPipeline;
        class VulkanCommandBuffer;
        class VulkanSwapchain;
        class VulkanDescriptorSet;
        class VulkanDescriptorSetLayout;
        class VulkanRenderPass;
        class VulkanFramebuffer;
        class VulkanShader;
    }

    namespace helios::rhi {
        using Context             = vulkan::VulkanContext;
        using Device              = vulkan::VulkanDevice;
        using Texture             = vulkan::VulkanTexture;
        using Buffer              = vulkan::VulkanBuffer;
        using Pipeline            = vulkan::VulkanPipeline;
        using CommandBuffer       = vulkan::VulkanCommandBuffer;
        using Swapchain           = vulkan::VulkanSwapchain;
        using DescriptorSet       = vulkan::VulkanDescriptorSet;
        using DescriptorSetLayout = vulkan::VulkanDescriptorSetLayout;
        using RenderPass          = vulkan::VulkanRenderPass;
        using Framebuffer         = vulkan::VulkanFramebuffer;
        using Shader              = vulkan::VulkanShader;
    }

#else
    #error "No rendering backend selected. Define HELIOS_BACKEND_VULKAN."
#endif
