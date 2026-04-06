#include "pch.h"
#include "Renderer/Vulkan/VulkanContext.h"

#include <GLFW/glfw3.h>

namespace Engine {

    vkb::Instance VulkanContext::s_Instance{};
    bool VulkanContext::s_ValidationEnabled = false;

    static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT severity,
        VkDebugUtilsMessageTypeFlagsEXT type,
        const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
        void* userData)
    {
        (void)type;
        (void)userData;

        const char* idName = callbackData->pMessageIdName ? callbackData->pMessageIdName : "Unknown";

        switch (severity) {
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
                HVE_CORE_TRACE_TAG("Vulkan", "[{}] {}", idName, callbackData->pMessage);
                break;
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
                HVE_CORE_INFO_TAG("Vulkan", "[{}] {}", idName, callbackData->pMessage);
                break;
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
                HVE_CORE_WARN_TAG("Vulkan", "[{}] {}", idName, callbackData->pMessage);
                break;
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
                HVE_CORE_ERROR_TAG("Vulkan", "[{}] {}", idName, callbackData->pMessage);
                break;
            default:
                HVE_CORE_TRACE_TAG("Vulkan", "[{}] {}", idName, callbackData->pMessage);
                break;
        }

        return VK_FALSE;
    }

    bool VulkanContext::Init(const char* appName, bool enableValidation)
    {
        s_ValidationEnabled = enableValidation;

        vkb::InstanceBuilder builder;
        builder.set_app_name(appName)
               .set_engine_name("Helios")
               .require_api_version(1, 3, 0);

        if (enableValidation) {
            builder.request_validation_layers()
                   .set_debug_callback(DebugCallback)
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

        auto result = builder.build();
        if (!result) {
            HVE_CORE_ERROR_TAG("Vulkan", "Failed to create Vulkan instance: {}", result.error().message());
            return false;
        }

        s_Instance = result.value();

        HVE_CORE_INFO_TAG("Vulkan", "Vulkan instance created (validation {})", enableValidation ? "enabled" : "disabled");
        return true;
    }

    void VulkanContext::Shutdown()
    {
        if (s_Instance.instance != VK_NULL_HANDLE) {
            vkb::destroy_debug_utils_messenger(s_Instance.instance, s_Instance.debug_messenger);
            vkb::destroy_instance(s_Instance);
            s_Instance = {};
            HVE_CORE_INFO_TAG("Vulkan", "Vulkan instance destroyed");
        }
    }

    VkSurfaceKHR VulkanContext::CreateSurface(GLFWwindow* window)
    {
        VkSurfaceKHR surface = VK_NULL_HANDLE;
        VkResult result = glfwCreateWindowSurface(s_Instance.instance, window, nullptr, &surface);
        if (result != VK_SUCCESS) {
            HVE_CORE_ERROR_TAG("Vulkan", "Failed to create window surface (VkResult: {})", static_cast<int>(result));
            return VK_NULL_HANDLE;
        }
        HVE_CORE_INFO_TAG("Vulkan", "Window surface created");
        return surface;
    }

    void VulkanContext::DestroySurface(VkSurfaceKHR surface)
    {
        if (surface != VK_NULL_HANDLE) {
            vkDestroySurfaceKHR(s_Instance.instance, surface, nullptr);
        }
    }

    void VulkanContext::SetDebugName(VkDevice device, VkObjectType type, uint64_t handle, const char* name)
    {
        if (!s_ValidationEnabled) return;

        VkDebugUtilsObjectNameInfoEXT info{};
        info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
        info.objectType = type;
        info.objectHandle = handle;
        info.pObjectName = name;

        auto func = reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(
            vkGetDeviceProcAddr(device, "vkSetDebugUtilsObjectNameEXT"));
        if (func) func(device, &info);
    }

}
