#pragma once

#include "helios/rhi/rhi_types.h"
#include <cstdint>
#include <memory>

namespace helios::rhi {

class CommandBuffer;

// Abstract swapchain interface.
class Swapchain {
public:
    virtual ~Swapchain() = default;

    Swapchain(const Swapchain&) = delete;
    Swapchain& operator=(const Swapchain&) = delete;

    virtual bool acquire_next_image() = 0;
    virtual void present() = 0;

    virtual uint32_t image_count() const = 0;
    virtual uint32_t current_image_index() const = 0;
    virtual uint32_t current_frame() const = 0;
    virtual uint32_t width() const = 0;
    virtual uint32_t height() const = 0;

    // Sync objects for the current frame-in-flight (opaque handles).
    // Vulkan: these are VkSemaphore / VkFence cast to void*.
    virtual void* image_available_semaphore() const = 0;
    virtual void* render_finished_semaphore() const = 0;
    virtual void* in_flight_fence() const = 0;

    // Convenience: begin rendering to the current swapchain image.
    // Handles image layout transitions and begins dynamic rendering with clear color.
    // Call after acquire_next_image() and cmd.begin().
    virtual void begin_rendering(CommandBuffer& cmd, const ClearValues& clear) = 0;

    // Convenience: end rendering to the current swapchain image.
    // Handles image layout transition to present-ready. Call before cmd.end().
    virtual void end_rendering(CommandBuffer& cmd) = 0;

protected:
    Swapchain() = default;
    Swapchain(Swapchain&&) noexcept = default;
    Swapchain& operator=(Swapchain&&) noexcept = default;
};

} // namespace helios::rhi
