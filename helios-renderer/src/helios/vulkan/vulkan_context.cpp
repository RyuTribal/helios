#include "helios/vulkan/vulkan_context.h"
#include "helios/vulkan/renderer_log_channels.h"

#include <helios/core/assert.h>

#include <GLFW/glfw3.h>
#include <utility>

namespace helios::rhi::vulkan {

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT /*type*/,
    const VkDebugUtilsMessengerCallbackDataEXT* data,
    void* /*user_data*/)
{
    const char* id = data->pMessageIdName ? data->pMessageIdName : "Unknown";
    switch (severity) {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
            HELIOS_LOG_TRACE(Renderer, "[{}] {}", id, data->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
            HELIOS_LOG_INFO(Renderer, "[{}] {}", id, data->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
            HELIOS_LOG_WARN(Renderer, "[{}] {}", id, data->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
            HELIOS_LOG_ERROR(Renderer, "[{}] {}", id, data->pMessage);
            break;
        default:
            HELIOS_LOG_TRACE(Renderer, "[{}] {}", id, data->pMessage);
            break;
    }
    return VK_FALSE;
}

VulkanContext::VulkanContext(const char* app_name, bool enable_validation)
    : m_validation_enabled(enable_validation)
{
    HELIOS_ASSERT(app_name != nullptr, "app_name must not be null");

    vkb::InstanceBuilder builder;
    builder.set_app_name(app_name)
           .set_engine_name("Helios")
           .require_api_version(1, 3, 0);

    if (enable_validation) {
        builder.request_validation_layers()
               .set_debug_callback(debug_callback)
               .set_debug_messenger_severity(
                   VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                   VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                   VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                   VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
               .set_debug_messenger_type(
                   VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                   VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                   VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT);
    }

    // Enable portability enumeration for MoltenVK / non-conformant drivers
    builder.enable_extension(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);

    auto result = builder.build();
    if (!result) {
        HELIOS_LOG_ERROR(Renderer, "Failed to create Vulkan instance: {}",
                         result.error().message());
        return;
    }

    m_instance = result.value();

    if (enable_validation && m_instance.debug_messenger == VK_NULL_HANDLE) {
        HELIOS_LOG_WARN(Renderer,
            "Validation layers requested but NOT loaded! "
            "Install vulkan-validation-layers (Arch: pacman -S vulkan-validation-layers)");
    }

    HELIOS_LOG_INFO(Renderer, "Vulkan instance created (validation {})",
                    (enable_validation && m_instance.debug_messenger != VK_NULL_HANDLE)
                        ? "enabled" : "disabled");
}

VulkanContext::~VulkanContext()
{
    destroy();
}

VulkanContext::VulkanContext(VulkanContext&& other) noexcept
    : m_instance(std::exchange(other.m_instance, vkb::Instance{}))
    , m_validation_enabled(std::exchange(other.m_validation_enabled, false))
{
}

VulkanContext& VulkanContext::operator=(VulkanContext&& other) noexcept
{
    if (this != &other) {
        destroy();
        m_instance = std::exchange(other.m_instance, vkb::Instance{});
        m_validation_enabled = std::exchange(other.m_validation_enabled, false);
    }
    return *this;
}

void VulkanContext::destroy()
{
    if (m_instance.instance != VK_NULL_HANDLE) {
        // Only destroy debug messenger if validation was enabled AND messenger exists
        if (m_instance.debug_messenger != VK_NULL_HANDLE) {
            vkb::destroy_debug_utils_messenger(m_instance.instance, m_instance.debug_messenger);
            m_instance.debug_messenger = VK_NULL_HANDLE;
        }
        vkb::destroy_instance(m_instance);
        m_instance = {};
        HELIOS_LOG_INFO(Renderer, "Vulkan instance destroyed");
    }
}

VkSurfaceKHR VulkanContext::create_surface(GLFWwindow* window) const
{
    HELIOS_ASSERT(window != nullptr, "window must not be null");
    HELIOS_ASSERT(m_instance.instance != VK_NULL_HANDLE, "VulkanContext not initialized");

    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkResult vk_result = glfwCreateWindowSurface(m_instance.instance, window, nullptr, &surface);
    if (vk_result != VK_SUCCESS) {
        HELIOS_LOG_ERROR(Renderer, "Failed to create window surface (VkResult: {})",
                         static_cast<int>(vk_result));
        return VK_NULL_HANDLE;
    }
    HELIOS_LOG_INFO(Renderer, "Window surface created");
    return surface;
}

void VulkanContext::destroy_surface(VkSurfaceKHR surface) const
{
    if (surface != VK_NULL_HANDLE && m_instance.instance != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(m_instance.instance, surface, nullptr);
    }
}

void VulkanContext::set_debug_name(VkDevice device, VkObjectType type,
                                   uint64_t handle, const char* name) const
{
    if (!m_validation_enabled) return;

    VkDebugUtilsObjectNameInfoEXT info{};
    info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    info.objectType = type;
    info.objectHandle = handle;
    info.pObjectName = name;

    auto func = reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(
        vkGetDeviceProcAddr(device, "vkSetDebugUtilsObjectNameEXT"));
    if (func) func(device, &info);
}

} // namespace helios::rhi::vulkan
