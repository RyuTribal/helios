#pragma once

#include "helios/rhi/rhi_types.h"
#include <memory>

namespace helios::rhi {

// Abstract descriptor set layout interface.
class DescriptorSetLayout {
public:
    virtual ~DescriptorSetLayout() = default;

    DescriptorSetLayout(const DescriptorSetLayout&) = delete;
    DescriptorSetLayout& operator=(const DescriptorSetLayout&) = delete;

protected:
    DescriptorSetLayout() = default;
    DescriptorSetLayout(DescriptorSetLayout&&) noexcept = default;
    DescriptorSetLayout& operator=(DescriptorSetLayout&&) noexcept = default;
};

// Abstract descriptor set interface.
class DescriptorSet {
public:
    virtual ~DescriptorSet() = default;

    DescriptorSet(const DescriptorSet&) = delete;
    DescriptorSet& operator=(const DescriptorSet&) = delete;

protected:
    DescriptorSet() = default;
    DescriptorSet(DescriptorSet&&) noexcept = default;
    DescriptorSet& operator=(DescriptorSet&&) noexcept = default;
};

} // namespace helios::rhi
