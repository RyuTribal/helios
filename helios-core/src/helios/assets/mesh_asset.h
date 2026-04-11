#pragma once

#include "helios/assets/handle.h"
#include "helios/assets/material_asset.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

namespace helios {

/// PBR vertex layout matching the engine's shaders.
struct PBRVertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;
    glm::vec4 tangent;  // xyz = tangent, w = handedness
};

/// CPU-side mesh asset. Stores vertex/index data and a default material reference.
struct MeshAsset {
    std::vector<PBRVertex> vertices;
    std::vector<uint32_t> indices;
    Handle<MaterialAsset> default_material;
};

} // namespace helios
