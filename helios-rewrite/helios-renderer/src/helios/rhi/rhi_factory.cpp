#include "helios/rhi/rhi_factory.h"
#include "helios/vulkan/vulkan_context.h"
#include "helios/vulkan/vulkan_device.h"
#include "helios/vulkan/renderer_log_channels.h"

#include <helios/core/assert.h>
#include <GLFW/glfw3.h>
#include <memory>

namespace helios::rhi {

// Internal: wraps VulkanContext ownership alongside VulkanDevice.
// VulkanDevice holds a non-owning pointer to VulkanContext, so we need
// something that owns both and exposes the Device interface.
class VulkanDeviceWithContext : public vulkan::VulkanDevice {
public:
    VulkanDeviceWithContext(std::unique_ptr<vulkan::VulkanContext> context,
                           VkSurfaceKHR surface)
        : vulkan::VulkanDevice(*context, surface)
        , m_owned_context(std::move(context))
        , m_owned_surface(surface)
    {
    }

    ~VulkanDeviceWithContext() override {
        // VulkanDevice::destroy() runs first (base destructor), then we clean up context/surface.
        // But we need to ensure the device is fully destroyed before the context.
        // The VulkanDevice destructor already calls destroy(), so by the time we get
        // here the device is gone. We just need to clean up the surface and context.
    }

    // Non-copyable, non-movable (always held via unique_ptr)
    VulkanDeviceWithContext(const VulkanDeviceWithContext&) = delete;
    VulkanDeviceWithContext& operator=(const VulkanDeviceWithContext&) = delete;
    VulkanDeviceWithContext(VulkanDeviceWithContext&&) = delete;
    VulkanDeviceWithContext& operator=(VulkanDeviceWithContext&&) = delete;

private:
    std::unique_ptr<vulkan::VulkanContext> m_owned_context;
    VkSurfaceKHR m_owned_surface = VK_NULL_HANDLE;
};

std::unique_ptr<Device> create_device(Backend backend,
                                      const char* app_name,
                                      GLFWwindow* window,
                                      bool enable_validation)
{
    HELIOS_ASSERT(app_name != nullptr, "App name must not be null");
    HELIOS_ASSERT(window != nullptr, "Window must not be null for device creation");

    switch (backend) {
        case Backend::Vulkan: {
            auto context = std::make_unique<vulkan::VulkanContext>(app_name, enable_validation);
            if (!*context) {
                HELIOS_LOG_ERROR(Renderer, "Failed to create Vulkan context");
                return nullptr;
            }

            VkSurfaceKHR surface = context->create_surface(window);
            if (surface == VK_NULL_HANDLE) {
                HELIOS_LOG_ERROR(Renderer, "Failed to create Vulkan surface");
                return nullptr;
            }

            auto device = std::make_unique<VulkanDeviceWithContext>(std::move(context), surface);
            if (!*device) {
                HELIOS_LOG_ERROR(Renderer, "Failed to create Vulkan device");
                return nullptr;
            }

            return device;
        }

        default:
            HELIOS_LOG_ERROR(Renderer, "Unsupported backend requested");
            return nullptr;
    }
}

Backend detect_best_backend()
{
    // For now, Vulkan is the only available backend.
    return Backend::Vulkan;
}

} // namespace helios::rhi
