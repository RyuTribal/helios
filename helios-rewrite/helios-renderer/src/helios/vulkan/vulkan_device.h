#pragma once

#include "helios/rhi/rhi_types.h"
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <functional>

namespace helios::rhi::vulkan {

class VulkanContext;
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

class VulkanDevice {
public:
    VulkanDevice(VulkanContext& context, VkSurfaceKHR surface);
    ~VulkanDevice();

    VulkanDevice(VulkanDevice&& other) noexcept;
    VulkanDevice& operator=(VulkanDevice&& other) noexcept;
    VulkanDevice(const VulkanDevice&) = delete;
    VulkanDevice& operator=(const VulkanDevice&) = delete;

    // Factory methods -- return RAII value types by value (moved out)
    // Declared here, implemented in future tasks (Task 16+).
    VulkanTexture create_texture(const TextureDesc& desc, const void* data = nullptr);
    VulkanBuffer create_buffer(const BufferDesc& desc, const void* data = nullptr);
    VulkanShader create_shader(const ShaderDesc& desc);
    // These are declared but not yet implemented:
    // VulkanPipeline create_graphics_pipeline(const GraphicsPipelineDesc& desc);
    // VulkanPipeline create_compute_pipeline(const ComputePipelineDesc& desc);
    // VulkanCommandBuffer create_command_buffer();
    // VulkanSwapchain create_swapchain(const SwapchainDesc& desc);
    // VulkanDescriptorSetLayout create_descriptor_set_layout(const DescriptorSetLayoutDesc& desc);
    // VulkanDescriptorSet create_descriptor_set(const VulkanDescriptorSetLayout& layout);
    // VulkanRenderPass create_render_pass(const RenderPassDesc& desc);
    // VulkanFramebuffer create_framebuffer(const FramebufferDesc& desc);
    // void update_descriptor_set(VulkanDescriptorSet& set,
    //                            const std::vector<DescriptorWrite>& writes);
    // void submit(const VulkanCommandBuffer& cmd, const SubmitInfo& info = {});

    void wait_idle();

    // One-shot command submission for transfers, layout transitions, etc.
    void immediate_submit(std::function<void(VkCommandBuffer)>&& fn);

    // Native handle escape hatch
    template<typename T> T native_handle() const;

    // Vulkan-specific accessors (used by other Vulkan backend types)
    VkDevice device() const { return m_device; }
    VkPhysicalDevice physical_device() const { return m_physical_device; }
    VkQueue graphics_queue() const { return m_graphics_queue; }
    uint32_t graphics_queue_family() const { return m_graphics_queue_family; }
    VmaAllocator allocator() const { return m_allocator; }
    VkDescriptorPool descriptor_pool() const { return m_descriptor_pool; }
    VulkanContext& context() const { return *m_context; }

    explicit operator bool() const { return m_device != VK_NULL_HANDLE; }

private:
    void destroy();

    VulkanContext* m_context                  = nullptr;  // non-owning
    VkPhysicalDevice m_physical_device       = VK_NULL_HANDLE;
    VkDevice m_device                        = VK_NULL_HANDLE;
    VkQueue m_graphics_queue                 = VK_NULL_HANDLE;
    uint32_t m_graphics_queue_family         = 0;
    VmaAllocator m_allocator                 = VK_NULL_HANDLE;
    VkDescriptorPool m_descriptor_pool       = VK_NULL_HANDLE;

    // Immediate submit resources
    VkCommandPool m_immediate_cmd_pool       = VK_NULL_HANDLE;
    VkCommandBuffer m_immediate_cmd_buffer   = VK_NULL_HANDLE;
    VkFence m_immediate_fence                = VK_NULL_HANDLE;
};

// Template specializations for native_handle
template<> inline VkDevice VulkanDevice::native_handle<VkDevice>() const {
    return m_device;
}
template<> inline VkPhysicalDevice VulkanDevice::native_handle<VkPhysicalDevice>() const {
    return m_physical_device;
}
template<> inline VmaAllocator VulkanDevice::native_handle<VmaAllocator>() const {
    return m_allocator;
}

} // namespace helios::rhi::vulkan
