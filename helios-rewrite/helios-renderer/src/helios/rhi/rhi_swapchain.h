#pragma once

#include <cstdint>
#include <memory>

namespace helios::rhi {

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

protected:
    Swapchain() = default;
    Swapchain(Swapchain&&) noexcept = default;
    Swapchain& operator=(Swapchain&&) noexcept = default;
};

} // namespace helios::rhi
