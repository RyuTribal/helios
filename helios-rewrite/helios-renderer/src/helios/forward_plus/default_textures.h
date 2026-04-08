// helios-renderer/src/helios/forward_plus/default_textures.h
//
// RAII default texture set. Constructor creates all placeholder textures via
// rhi::Device, destructor releases them automatically through unique_ptr.
// NOT a singleton -- inserted as a World resource by ForwardPlusPlugin.
#pragma once

#include "helios/rhi/rhi_device.h"
#include "helios/rhi/rhi_texture.h"

#include <memory>

namespace helios {

class DefaultTextures {
public:
    explicit DefaultTextures(rhi::Device& device);
    ~DefaultTextures() = default;

    DefaultTextures(DefaultTextures&&) noexcept = default;
    DefaultTextures& operator=(DefaultTextures&&) noexcept = default;
    DefaultTextures(const DefaultTextures&) = delete;
    DefaultTextures& operator=(const DefaultTextures&) = delete;

    /// 1x1 white RGBA8 texture (default albedo).
    const rhi::Texture& white() const { return *m_white; }

    /// 1x1 black RGBA8 texture (default emission/AO).
    const rhi::Texture& black() const { return *m_black; }

    /// 1x1 flat-normal blue RGBA8 texture (128,128,255,255).
    const rhi::Texture& blue() const { return *m_blue; }

    /// 1x1 black cubemap (6 faces) -- placeholder for missing environment maps.
    const rhi::Texture& black_cube() const { return *m_black_cube; }

    /// 1x1 white 2D array (1 layer) -- placeholder for sampler2DArray bindings.
    const rhi::Texture& white_array() const { return *m_white_array; }

    /// 1x1 BRDF LUT placeholder (R=0.5, G=0.0 for reasonable specular fallback).
    const rhi::Texture& brdf_lut_placeholder() const { return *m_brdf_lut; }

private:
    std::unique_ptr<rhi::Texture> m_white;
    std::unique_ptr<rhi::Texture> m_black;
    std::unique_ptr<rhi::Texture> m_blue;
    std::unique_ptr<rhi::Texture> m_black_cube;
    std::unique_ptr<rhi::Texture> m_white_array;
    std::unique_ptr<rhi::Texture> m_brdf_lut;
};

} // namespace helios
