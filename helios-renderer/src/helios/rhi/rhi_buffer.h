#pragma once

#include "helios/rhi/rhi_types.h"
#include <cstdint>
#include <memory>

namespace helios::rhi {

// Abstract buffer interface.
class Buffer {
public:
    virtual ~Buffer() = default;

    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    virtual void set_data(const void* data, uint32_t size, uint32_t offset = 0) = 0;
    virtual void* map() = 0;
    virtual void unmap() = 0;
    virtual uint32_t size() const = 0;
    virtual const BufferDesc& desc() const = 0;

    template<typename T> T native_handle() const;

protected:
    Buffer() = default;
    Buffer(Buffer&&) noexcept = default;
    Buffer& operator=(Buffer&&) noexcept = default;
};

} // namespace helios::rhi
