#pragma once

#include "helios/rhi/rhi_types.h"
#include <memory>

namespace helios::rhi {

// Abstract render pass interface.
class RenderPass {
public:
    virtual ~RenderPass() = default;

    RenderPass(const RenderPass&) = delete;
    RenderPass& operator=(const RenderPass&) = delete;

    virtual const RenderPassDesc& desc() const = 0;

protected:
    RenderPass() = default;
    RenderPass(RenderPass&&) noexcept = default;
    RenderPass& operator=(RenderPass&&) noexcept = default;
};

} // namespace helios::rhi
