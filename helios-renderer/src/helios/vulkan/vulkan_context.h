#pragma once

#include <vulkan/vulkan.h>
#include <VkBootstrap.h>
#include <string>

struct GLFWwindow;

namespace helios::rhi::vulkan {

// RAII Vulkan instance wrapper.
// Constructor creates VkInstance + debug messenger (if validation enabled).
// Destructor destroys both. Move-only.
class VulkanContext {
public:
    VulkanContext(const char* app_name, bool enable_validation);
    ~VulkanContext();

    VulkanContext(VulkanContext&& other) noexcept;
    VulkanContext& operator=(VulkanContext&& other) noexcept;
    VulkanContext(const VulkanContext&) = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;

    [[nodiscard]] VkInstance instance() const { return m_instance.instance; }
    [[nodiscard]] const vkb::Instance& vkb_instance() const { return m_instance; }
    [[nodiscard]] bool validation_enabled() const { return m_validation_enabled; }

    // Surface creation/destruction (wraps glfwCreateWindowSurface)
    [[nodiscard]] VkSurfaceKHR create_surface(GLFWwindow* window) const;
    void destroy_surface(VkSurfaceKHR surface) const;

    // Debug object naming -- call on every Vulkan object for better validation messages
    void set_debug_name(VkDevice device, VkObjectType type,
                        uint64_t handle, const char* name) const;

    explicit operator bool() const { return m_instance.instance != VK_NULL_HANDLE; }

private:
    void destroy();

    vkb::Instance m_instance{};
    bool m_validation_enabled = false;
};

} // namespace helios::rhi::vulkan
