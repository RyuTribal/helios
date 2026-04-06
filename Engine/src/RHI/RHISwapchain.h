#pragma once

#include "RHI/RHITypes.h"
#include "RHI/RHIResources.h"

namespace Engine {

    class RHISwapchain {
    public:
        virtual ~RHISwapchain() = default;
        virtual bool AcquireNextImage() = 0;
        virtual void Present() = 0;
        virtual void Resize(uint32_t width, uint32_t height) = 0;
        virtual RHITexture* GetCurrentImage() = 0;
        virtual uint32_t GetImageCount() const = 0;
        virtual uint32_t GetCurrentImageIndex() const = 0;
        virtual uint32_t GetWidth() const = 0;
        virtual uint32_t GetHeight() const = 0;
        virtual ImageFormat GetFormat() const = 0;
    };

} // namespace Engine
