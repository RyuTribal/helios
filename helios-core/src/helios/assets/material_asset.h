#pragma once

#include "helios/assets/handle.h"
#include "helios/assets/texture_asset.h"

#include <glm/glm.hpp>

namespace helios {

/// CPU-side PBR material asset. References textures via typed handles.
struct MaterialAsset {
    Handle<TextureAsset> albedo;
    Handle<TextureAsset> normal;
    Handle<TextureAsset> metallic_roughness;
    Handle<TextureAsset> emissive;
    glm::vec3 base_color{1.0f};
    float metallic = 1.0f;
    float roughness = 1.0f;
};

} // namespace helios
