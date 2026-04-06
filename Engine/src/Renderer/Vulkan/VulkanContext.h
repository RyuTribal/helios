#pragma once

#include <vulkan/vulkan.h>
#include <VkBootstrap.h>

struct GLFWwindow;

namespace Engine {
    class VulkanContext {
    public:
        static bool Init(const char* appName, bool enableValidation);
        static void Shutdown();

        static VkInstance GetInstance() { return s_Instance.instance; }
        static const vkb::Instance& GetVkbInstance() { return s_Instance; }
        static VkSurfaceKHR CreateSurface(GLFWwindow* window);
        static void DestroySurface(VkSurfaceKHR surface);

        // Debug object naming -- call on every Vulkan object creation for better validation messages
        static void SetDebugName(VkDevice device, VkObjectType type, uint64_t handle, const char* name);

        static bool IsValidationEnabled() { return s_ValidationEnabled; }

    private:
        static vkb::Instance s_Instance;
        static bool s_ValidationEnabled;
    };
}
