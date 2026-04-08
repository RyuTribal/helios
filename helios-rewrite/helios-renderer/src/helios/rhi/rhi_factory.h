#pragma once

#include "helios/rhi/rhi_device.h"
#include <memory>
#include <string>
#include <vector>
#include <cstdint>

struct GLFWwindow;

namespace helios::rhi {

// Available rendering backends.
enum class Backend {
    Vulkan,
    // D3D12,  // future
};

// GPU type classification.
enum class GpuType {
    Discrete,     // dedicated GPU (NVIDIA, AMD)
    Integrated,   // integrated in CPU (Intel, AMD APU)
    Virtual,      // virtual/software
    Other,
};

// Information about a GPU available on the system.
struct GpuInfo {
    uint32_t    index = 0;          // pass to create_device() to select this GPU
    std::string name;               // human-readable: "NVIDIA GeForce RTX 4090"
    GpuType     type = GpuType::Other;
    uint64_t    vram_bytes = 0;     // dedicated VRAM (0 for integrated)
    uint32_t    vendor_id = 0;
    uint32_t    device_id = 0;
    std::string driver_version;     // e.g., "545.29.06"
    std::string api_version;        // e.g., "1.3.280"
};

// Enumerate all GPUs available for a given backend.
// This is lightweight — does NOT create a device, just queries the system.
// For Vulkan: creates a temporary VkInstance, enumerates physical devices, destroys instance.
std::vector<GpuInfo> enumerate_devices(Backend backend);

// Create a device on a specific GPU (by index from enumerate_devices).
// If gpu_index is UINT32_MAX, auto-selects the best available GPU.
std::unique_ptr<Device> create_device(Backend backend,
                                      const char* app_name,
                                      GLFWwindow* window,
                                      uint32_t gpu_index = UINT32_MAX,
                                      bool enable_validation = true);

// Auto-detect the best available backend for this platform.
Backend detect_best_backend();

} // namespace helios::rhi
