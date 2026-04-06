#pragma once

#include "RHI/RHITypes.h"
#include "RHI/RHIResources.h"

namespace Engine {

    class RHIDescriptorSetLayout {
    public:
        virtual ~RHIDescriptorSetLayout() = default;
    };

    class RHIDescriptorSet {
    public:
        virtual ~RHIDescriptorSet() = default;
    };

    struct DescriptorWrite {
        uint32_t Binding = 0;
        DescriptorType Type = DescriptorType::UniformBuffer;
        RHIBuffer* Buffer = nullptr;
        RHITexture* Texture = nullptr;
        uint32_t Offset = 0;
        uint32_t Range = 0;
    };

} // namespace Engine
