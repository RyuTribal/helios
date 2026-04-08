#pragma once

#include "helios/rhi/rhi_device.h"
#include <memory>

struct GLFWwindow;

namespace helios::rhi {

// Available rendering backends.
enum class Backend {
    Vulkan,
    // D3D12,  // future
};

// Create a rendering context and device for the given backend.
// The returned Device owns all backend state and is the primary factory
// for GPU resources.
//
// For Vulkan: creates VulkanContext + VulkanDevice internally.
// The window parameter is needed to create a surface for device selection.
std::unique_ptr<Device> create_device(Backend backend,
                                      const char* app_name,
                                      GLFWwindow* window,
                                      bool enable_validation = true);

// Auto-detect the best available backend for this platform.
Backend detect_best_backend();

} // namespace helios::rhi
