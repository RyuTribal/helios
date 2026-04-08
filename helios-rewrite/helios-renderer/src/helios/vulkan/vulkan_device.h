#pragma once

#include "helios/rhi/rhi_device.h"
#include "helios/rhi/rhi_swapchain.h"
#include "helios/rhi/rhi_types.h"
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <functional>
#include <vector>

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

class VulkanDevice : public rhi::Device {
public:
    // gpu_index: UINT32_MAX = auto-select best, otherwise index from rhi::enumerate_devices()
    VulkanDevice(VulkanContext& context, VkSurfaceKHR surface, uint32_t gpu_index = UINT32_MAX);
    ~VulkanDevice() override;

    VulkanDevice(VulkanDevice&& other) noexcept;
    VulkanDevice& operator=(VulkanDevice&& other) noexcept;
    VulkanDevice(const VulkanDevice&) = delete;
    VulkanDevice& operator=(const VulkanDevice&) = delete;

    // rhi::Device interface -- factory methods returning unique_ptr to abstract types
    std::unique_ptr<rhi::Texture> create_texture(const TextureDesc& desc,
                                                 const void* data = nullptr) override;
    std::unique_ptr<rhi::Buffer> create_buffer(const BufferDesc& desc,
                                               const void* data = nullptr) override;
    std::unique_ptr<rhi::Shader> create_shader(const ShaderDesc& desc) override;
    std::unique_ptr<rhi::Pipeline> create_graphics_pipeline(const GraphicsPipelineDesc& desc) override;
    std::unique_ptr<rhi::Pipeline> create_compute_pipeline(const ComputePipelineDesc& desc) override;
    std::unique_ptr<rhi::CommandBuffer> create_command_buffer() override;
    std::unique_ptr<rhi::Swapchain> create_swapchain(const SwapchainDesc& desc) override;
    std::unique_ptr<rhi::RenderPass> create_render_pass(const RenderPassDesc& desc) override;
    std::unique_ptr<rhi::Framebuffer> create_framebuffer(const FramebufferDesc& desc) override;
    std::unique_ptr<rhi::DescriptorSetLayout> create_descriptor_set_layout(
        const DescriptorSetLayoutDesc& desc) override;
    std::unique_ptr<rhi::DescriptorSet> allocate_descriptor_set(
        const rhi::DescriptorSetLayout& layout) override;
    void update_descriptor_set(rhi::DescriptorSet& set,
                               const std::vector<DescriptorWrite>& writes) override;
    void submit(const rhi::CommandBuffer& cmd, const SubmitInfo& info = {}) override;
    void wait_idle() override;

    // Surface management
    void* create_surface(void* native_window) override;
    void destroy_surface(void* surface) override;

    // Submit with swapchain sync
    void submit_for_present(const rhi::CommandBuffer& cmd, const rhi::Swapchain& swapchain) override;

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
    VkDescriptorPool descriptor_pool() const {
        return m_descriptor_pools.empty() ? VK_NULL_HANDLE : m_descriptor_pools.back();
    }
    VulkanContext& context() const { return *m_context; }

    explicit operator bool() const { return m_device != VK_NULL_HANDLE; }

protected:
    void destroy();

    VulkanContext* m_context                  = nullptr;  // non-owning
    VkPhysicalDevice m_physical_device       = VK_NULL_HANDLE;
    VkDevice m_device                        = VK_NULL_HANDLE;
    VkQueue m_graphics_queue                 = VK_NULL_HANDLE;
    uint32_t m_graphics_queue_family         = 0;
    VmaAllocator m_allocator                 = VK_NULL_HANDLE;
    std::vector<VkDescriptorPool> m_descriptor_pools;

    // Immediate submit resources
    VkCommandPool m_immediate_cmd_pool       = VK_NULL_HANDLE;
    VkCommandBuffer m_immediate_cmd_buffer   = VK_NULL_HANDLE;
    VkFence m_immediate_fence                = VK_NULL_HANDLE;

    VkDescriptorPool create_descriptor_pool();
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
