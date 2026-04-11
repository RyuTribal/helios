#pragma once

#include <memory>

namespace helios::rhi {

// Abstract pipeline interface (graphics or compute).
class Pipeline {
public:
    virtual ~Pipeline() = default;

    Pipeline(const Pipeline&) = delete;
    Pipeline& operator=(const Pipeline&) = delete;

protected:
    Pipeline() = default;
    Pipeline(Pipeline&&) noexcept = default;
    Pipeline& operator=(Pipeline&&) noexcept = default;
};

} // namespace helios::rhi
