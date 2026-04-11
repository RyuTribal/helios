#pragma once

#include "helios/rhi/rhi_types.h"
#include <memory>

namespace helios::rhi {

// Abstract shader module interface.
class Shader {
public:
    virtual ~Shader() = default;

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    virtual ShaderStage stage() const = 0;
    virtual const char* entry_point() const = 0;

protected:
    Shader() = default;
    Shader(Shader&&) noexcept = default;
    Shader& operator=(Shader&&) noexcept = default;
};

} // namespace helios::rhi
