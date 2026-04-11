#include "helios/rhi/rhi_factory.h"
#include "helios/vulkan/vulkan_context.h"
#include "helios/vulkan/vulkan_device.h"
#include "helios/vulkan/renderer_log_channels.h"

#include <helios/core/assert.h>
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>
#include <memory>

namespace helios::rhi {

// ============================================================================
// Internal: wraps VulkanContext ownership alongside VulkanDevice
// ============================================================================

class VulkanDeviceWithContext : public vulkan::VulkanDevice {
public:
    VulkanDeviceWithContext(std::unique_ptr<vulkan::VulkanContext> context,
                           VkSurfaceKHR surface,
                           uint32_t gpu_index)
        : vulkan::VulkanDevice(*context, surface, gpu_index)
        , m_owned_context(std::move(context))
        , m_owned_surface(surface)
    {}

    ~VulkanDeviceWithContext() override {
        // Correct destruction order: device first, then surface, then context.
        destroy();  // VulkanDevice::destroy() -- idempotent

        if (m_owned_surface != VK_NULL_HANDLE && m_owned_context) {
            m_owned_context->destroy_surface(m_owned_surface);
            m_owned_surface = VK_NULL_HANDLE;
        }
        // m_owned_context destroyed by unique_ptr member dtor (VkInstance last)
    }

    VulkanDeviceWithContext(const VulkanDeviceWithContext&) = delete;
    VulkanDeviceWithContext& operator=(const VulkanDeviceWithContext&) = delete;
    VulkanDeviceWithContext(VulkanDeviceWithContext&&) = delete;
    VulkanDeviceWithContext& operator=(VulkanDeviceWithContext&&) = delete;

    // Override create_swapchain: if desc.surface is null, use the primary surface
    // that was created during device initialization.
    std::unique_ptr<rhi::Swapchain> create_swapchain(const SwapchainDesc& desc) override {
        if (desc.surface == nullptr) {
            SwapchainDesc patched = desc;
            patched.surface = m_owned_surface;
            return vulkan::VulkanDevice::create_swapchain(patched);
        }
        return vulkan::VulkanDevice::create_swapchain(desc);
    }

private:
    std::unique_ptr<vulkan::VulkanContext> m_owned_context;
    VkSurfaceKHR m_owned_surface = VK_NULL_HANDLE;
};

// ============================================================================
// enumerate_devices — lightweight GPU enumeration
// ============================================================================

static GpuType vk_device_type_to_gpu_type(VkPhysicalDeviceType vk_type) {
    switch (vk_type) {
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:   return GpuType::Discrete;
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return GpuType::Integrated;
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:    return GpuType::Virtual;
        default: return GpuType::Other;
    }
}

static std::string vk_version_to_string(uint32_t version) {
    return std::to_string(VK_API_VERSION_MAJOR(version)) + "." +
           std::to_string(VK_API_VERSION_MINOR(version)) + "." +
           std::to_string(VK_API_VERSION_PATCH(version));
}

std::vector<GpuInfo> enumerate_devices(Backend backend) {
    std::vector<GpuInfo> result;

    switch (backend) {
        case Backend::Vulkan: {
            // Create a temporary Vulkan instance just for enumeration
            VkApplicationInfo app_info{};
            app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
            app_info.pApplicationName = "helios_enumerate";
            app_info.apiVersion = VK_API_VERSION_1_3;

            VkInstanceCreateInfo create_info{};
            create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
            create_info.pApplicationInfo = &app_info;

#ifdef VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME
            const char* extensions[] = { VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME };
            create_info.enabledExtensionCount = 1;
            create_info.ppEnabledExtensionNames = extensions;
            create_info.flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif

            VkInstance instance = VK_NULL_HANDLE;
            VkResult vk_result = vkCreateInstance(&create_info, nullptr, &instance);
            if (vk_result != VK_SUCCESS) {
                HELIOS_LOG(Renderer, Warn, "Failed to create temp Vulkan instance for enumeration (VkResult={})", static_cast<int>(vk_result));
                return result;
            }

            uint32_t count = 0;
            vkEnumeratePhysicalDevices(instance, &count, nullptr);
            std::vector<VkPhysicalDevice> physical_devices(count);
            vkEnumeratePhysicalDevices(instance, &count, physical_devices.data());

            for (uint32_t i = 0; i < count; ++i) {
                VkPhysicalDeviceProperties props{};
                vkGetPhysicalDeviceProperties(physical_devices[i], &props);

                VkPhysicalDeviceMemoryProperties mem_props{};
                vkGetPhysicalDeviceMemoryProperties(physical_devices[i], &mem_props);

                // Sum up device-local memory heaps
                uint64_t vram = 0;
                for (uint32_t h = 0; h < mem_props.memoryHeapCount; ++h) {
                    if (mem_props.memoryHeaps[h].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
                        vram += mem_props.memoryHeaps[h].size;
                    }
                }

                GpuInfo info;
                info.index = i;
                info.name = props.deviceName;
                info.type = vk_device_type_to_gpu_type(props.deviceType);
                info.vram_bytes = vram;
                info.vendor_id = props.vendorID;
                info.device_id = props.deviceID;
                info.driver_version = vk_version_to_string(props.driverVersion);
                info.api_version = vk_version_to_string(props.apiVersion);

                result.push_back(std::move(info));
            }

            vkDestroyInstance(instance, nullptr);
            break;
        }

        default:
            break;
    }

    return result;
}

// ============================================================================
// create_device
// ============================================================================

std::unique_ptr<Device> create_device(Backend backend,
                                      const char* app_name,
                                      void* native_window,
                                      uint32_t gpu_index,
                                      bool enable_validation)
{
    HELIOS_ASSERT(app_name != nullptr, "App name must not be null");
    HELIOS_ASSERT(native_window != nullptr, "Window must not be null for device creation");
    auto* window = static_cast<GLFWwindow*>(native_window);

    switch (backend) {
        case Backend::Vulkan: {
            auto context = std::make_unique<vulkan::VulkanContext>(app_name, enable_validation);
            if (!*context) {
                HELIOS_LOG(Renderer, Error, "Failed to create Vulkan context");
                return nullptr;
            }

            VkSurfaceKHR surface = context->create_surface(window);
            if (surface == VK_NULL_HANDLE) {
                HELIOS_LOG(Renderer, Error, "Failed to create Vulkan surface");
                return nullptr;
            }

            auto device = std::make_unique<VulkanDeviceWithContext>(
                std::move(context), surface, gpu_index);
            if (!*device) {
                HELIOS_LOG(Renderer, Error, "Failed to create Vulkan device");
                return nullptr;
            }

            return device;
        }

        default:
            HELIOS_LOG(Renderer, Error, "Unsupported backend requested");
            return nullptr;
    }
}

// ============================================================================
// detect_best_backend
// ============================================================================

Backend detect_best_backend() {
    return Backend::Vulkan;
}

} // namespace helios::rhi
