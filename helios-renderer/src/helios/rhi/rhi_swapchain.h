#pragma once

#include "helios/rhi/rhi_types.h"
#include <cstdint>
#include <memory>

namespace helios::rhi {

class CommandBuffer;
class Texture;

// Abstract swapchain interface.
class Swapchain {
public:
    virtual ~Swapchain() = default;

    Swapchain(const Swapchain&) = delete;
    Swapchain& operator=(const Swapchain&) = delete;

    virtual bool acquire_next_image() = 0;
    virtual bool present() = 0;  // returns false if swapchain needs recreation

    virtual uint32_t image_count() const = 0;
    virtual uint32_t current_image_index() const = 0;
    virtual uint32_t current_frame() const = 0;
    virtual uint32_t width() const = 0;
    virtual uint32_t height() const = 0;
    virtual TextureFormat color_format() const = 0;

    // Sync objects for the current frame-in-flight (opaque handles).
    // Vulkan: these are VkSemaphore / VkFence cast to void*.
    virtual void* image_available_semaphore() const = 0;
    virtual void* render_finished_semaphore() const = 0;
    virtual void* in_flight_fence() const = 0;

    // Convenience: begin rendering to the current swapchain image.
    // Handles image layout transitions and begins dynamic rendering with clear color.
    // Call after acquire_next_image() and cmd.begin().
    // If depth_attachment is provided, a depth buffer is attached to the rendering.
    virtual void begin_rendering(CommandBuffer& cmd, const ClearValues& clear,
                                 Texture* depth_attachment = nullptr) = 0;

    // Convenience: end rendering to the current swapchain image.
    // Handles image layout transition to present-ready. Call before cmd.end().
    virtual void end_rendering(CommandBuffer& cmd) = 0;

    // Blit a source texture onto the current swapchain image and transition to present.
    // Handles all layout transitions internally (src → TransferSrc, swapchain → TransferDst → PresentSrc).
    virtual void blit_from(CommandBuffer& cmd, Texture& src,
                           uint32_t src_width, uint32_t src_height) = 0;

protected:
    Swapchain() = default;
    Swapchain(Swapchain&&) noexcept = default;
    Swapchain& operator=(Swapchain&&) noexcept = default;
};

} // namespace helios::rhi
