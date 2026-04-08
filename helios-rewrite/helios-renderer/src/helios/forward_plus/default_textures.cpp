// helios-renderer/src/helios/forward_plus/default_textures.cpp
#include "helios/forward_plus/default_textures.h"
#include "helios/forward_plus/forward_plus_log_channel.h"
#include "helios/core/assert.h"

namespace helios {

DefaultTextures::DefaultTextures(rhi::Device& device) {
    using namespace rhi;

    // 1x1 RGBA8 white
    {
        uint32_t data = 0xFFFFFFFF;
        TextureDesc desc{
            .width      = 1,
            .height     = 1,
            .format     = TextureFormat::RGBA8,
            .type       = TextureType::Texture2D,
            .mip_levels = 1,
            .usage      = TextureUsage::Sampled | TextureUsage::Transfer,
            .debug_name = "DefaultWhite",
        };
        m_white = device.create_texture(desc, &data);
        HELIOS_ASSERT(m_white, "Failed to create default white texture");
    }

    // 1x1 RGBA8 black
    {
        uint32_t data = 0xFF000000; // ABGR: A=255, B=0, G=0, R=0
        TextureDesc desc{
            .width      = 1,
            .height     = 1,
            .format     = TextureFormat::RGBA8,
            .type       = TextureType::Texture2D,
            .mip_levels = 1,
            .usage      = TextureUsage::Sampled | TextureUsage::Transfer,
            .debug_name = "DefaultBlack",
        };
        m_black = device.create_texture(desc, &data);
        HELIOS_ASSERT(m_black, "Failed to create default black texture");
    }

    // 1x1 RGBA8 flat-normal blue (128, 128, 255, 255)
    {
        uint32_t data = 0xFFFF8080; // ABGR: A=255, B=255, G=128, R=128
        TextureDesc desc{
            .width      = 1,
            .height     = 1,
            .format     = TextureFormat::RGBA8,
            .type       = TextureType::Texture2D,
            .mip_levels = 1,
            .usage      = TextureUsage::Sampled | TextureUsage::Transfer,
            .debug_name = "DefaultBlue",
        };
        m_blue = device.create_texture(desc, &data);
        HELIOS_ASSERT(m_blue, "Failed to create default blue texture");
    }

    // 1x1 black cubemap (6 faces)
    {
        uint32_t faces[6] = {
            0xFF000000, 0xFF000000, 0xFF000000,
            0xFF000000, 0xFF000000, 0xFF000000,
        };
        TextureDesc desc{
            .width        = 1,
            .height       = 1,
            .format       = TextureFormat::RGBA8,
            .type         = TextureType::TextureCube,
            .mip_levels   = 1,
            .array_layers = 6,
            .usage        = TextureUsage::Sampled | TextureUsage::Transfer,
            .debug_name   = "DefaultBlackCube",
        };
        m_black_cube = device.create_texture(desc, faces);
        HELIOS_ASSERT(m_black_cube, "Failed to create default black cubemap");
    }

    // 1x1 white 2D array (1 layer) for sampler2DArray bindings
    {
        uint32_t data = 0xFFFFFFFF;
        TextureDesc desc{
            .width        = 1,
            .height       = 1,
            .format       = TextureFormat::RGBA8,
            .type         = TextureType::Texture2DArray,
            .mip_levels   = 1,
            .array_layers = 1,
            .usage        = TextureUsage::Sampled | TextureUsage::Transfer,
            .debug_name   = "DefaultWhiteArray",
        };
        m_white_array = device.create_texture(desc, &data);
        HELIOS_ASSERT(m_white_array, "Failed to create default white array texture");
    }

    // 1x1 BRDF LUT placeholder: R=0.5 (128), G=0.0 (0) for reasonable specular fallback
    {
        uint32_t data = 0xFF000080; // ABGR: A=255, B=0, G=0, R=128
        TextureDesc desc{
            .width      = 1,
            .height     = 1,
            .format     = TextureFormat::RGBA8,
            .type       = TextureType::Texture2D,
            .mip_levels = 1,
            .usage      = TextureUsage::Sampled | TextureUsage::Transfer,
            .debug_name = "DefaultBRDF",
        };
        m_brdf_lut = device.create_texture(desc, &data);
        HELIOS_ASSERT(m_brdf_lut, "Failed to create default BRDF LUT texture");
    }

    HELIOS_LOG_INFO(ForwardPlus, "DefaultTextures created (6 placeholder textures)");
}

} // namespace helios
