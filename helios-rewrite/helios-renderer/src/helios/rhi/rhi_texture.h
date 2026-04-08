#pragma once

#include "helios/rhi/rhi_types.h"
#include <cstdint>
#include <memory>

namespace helios::rhi {

// Abstract texture interface.
// Backend implementations (VulkanTexture, etc.) inherit from this.
class Texture {
public:
    virtual ~Texture() = default;

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    virtual uint32_t width() const = 0;
    virtual uint32_t height() const = 0;
    virtual TextureFormat format() const = 0;
    virtual const TextureDesc& desc() const = 0;

    // Native handle escape hatch.
    // Usage: texture->native_handle<VkImage>()
    // Returns nullptr/null-handle if the requested type does not match the backend.
    template<typename T> T native_handle() const;

protected:
    Texture() = default;
    Texture(Texture&&) noexcept = default;
    Texture& operator=(Texture&&) noexcept = default;
};

} // namespace helios::rhi
