#pragma once

#include <cstdint>
#include <vector>

namespace helios {

/// Raw SPIR-V shader bytecode loaded from a .spv file.
struct ShaderAsset {
    std::vector<uint8_t> spirv;  // raw SPIR-V bytes
};

} // namespace helios
