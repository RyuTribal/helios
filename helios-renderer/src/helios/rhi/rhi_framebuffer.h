#pragma once

#include <cstdint>
#include <memory>

namespace helios::rhi {

// Abstract framebuffer interface.
class Framebuffer {
public:
    virtual ~Framebuffer() = default;

    Framebuffer(const Framebuffer&) = delete;
    Framebuffer& operator=(const Framebuffer&) = delete;

    virtual uint32_t width() const = 0;
    virtual uint32_t height() const = 0;

protected:
    Framebuffer() = default;
    Framebuffer(Framebuffer&&) noexcept = default;
    Framebuffer& operator=(Framebuffer&&) noexcept = default;
};

} // namespace helios::rhi
