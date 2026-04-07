#include "pch.h"
#include "DefaultTextures.h"

namespace Engine {

    Ref<RHITexture> DefaultTextures::s_White;
    Ref<RHITexture> DefaultTextures::s_Black;
    Ref<RHITexture> DefaultTextures::s_Gray;
    Ref<RHITexture> DefaultTextures::s_Blue;
    Ref<RHITexture> DefaultTextures::s_BlackCube;
    Ref<RHITexture> DefaultTextures::s_WhiteArray;
    Ref<RHITexture> DefaultTextures::s_BrdfLUT;

    void DefaultTextures::Init(RHIDevice* device)
    {
        TextureDesc desc;
        desc.Width = 1;
        desc.Height = 1;
        desc.Format = ImageFormat::RGBA8;
        desc.Type = TextureType::Texture2D;
        desc.MipLevels = 1;
        desc.Usage = TextureUsage::Sampled | TextureUsage::Transfer;

        uint32_t whiteData = 0xFFFFFFFF;
        desc.DebugName = "DefaultWhite";
        s_White = device->CreateTexture(desc, &whiteData);

        // BRDF LUT stand-in: R≈0.5 G≈0.0 gives reasonable specular without proper integration
        // (used as sampler2D binding 12 in forward pass — brdf.rg)
        uint32_t brdfData = 0xFF000080; // ABGR: R=128(0.5), G=0, B=0, A=255
        desc.DebugName = "DefaultBRDF";
        s_BrdfLUT = device->CreateTexture(desc, &brdfData);

        uint32_t blackData = 0xFF000000;
        desc.DebugName = "DefaultBlack";
        s_Black = device->CreateTexture(desc, &blackData);

        uint32_t grayData = 0xFF808080;
        desc.DebugName = "DefaultGray";
        s_Gray = device->CreateTexture(desc, &grayData);

        // Normal-map default: (128, 128, 255, 255) = flat normal pointing up
        uint32_t blueData = 0xFFFF8080;
        desc.DebugName = "DefaultBlue";
        s_Blue = device->CreateTexture(desc, &blueData);

        // 1x1 white cubemap (6 faces, RGBA8) — provides basic ambient lighting
        {
            TextureDesc cubeDesc;
            cubeDesc.Width = 1;
            cubeDesc.Height = 1;
            cubeDesc.Format = ImageFormat::RGBA8;
            cubeDesc.Type = TextureType::TextureCube;
            cubeDesc.MipLevels = 1;
            cubeDesc.ArrayLayers = 6;
            cubeDesc.Usage = TextureUsage::Sampled | TextureUsage::Transfer;
            cubeDesc.DebugName = "DefaultBlackCube";
            uint32_t blackFaces[6] = { 0xFF000000, 0xFF000000, 0xFF000000,
                                       0xFF000000, 0xFF000000, 0xFF000000 };
            s_BlackCube = device->CreateTexture(cubeDesc, blackFaces);
        }

        // 1x1 white 2D array (1 layer, RGBA8) — for sampler2DArray bindings
        {
            TextureDesc arrDesc;
            arrDesc.Width = 1;
            arrDesc.Height = 1;
            arrDesc.Format = ImageFormat::RGBA8;
            arrDesc.Type = TextureType::Texture2DArray;
            arrDesc.MipLevels = 1;
            arrDesc.ArrayLayers = 1;
            arrDesc.Usage = TextureUsage::Sampled | TextureUsage::Transfer;
            arrDesc.DebugName = "DefaultWhiteArray";
            uint32_t whiteArr = 0xFFFFFFFF;
            s_WhiteArray = device->CreateTexture(arrDesc, &whiteArr);
        }

        HVE_CORE_INFO_TAG("DefaultTextures", "Default textures initialized");
    }

    void DefaultTextures::Shutdown()
    {
        s_White.reset();
        s_Black.reset();
        s_Gray.reset();
        s_Blue.reset();
        s_BlackCube.reset();
        s_WhiteArray.reset();
        s_BrdfLUT.reset();
    }

    Ref<RHITexture> DefaultTextures::White() { return s_White; }
    Ref<RHITexture> DefaultTextures::Black() { return s_Black; }
    Ref<RHITexture> DefaultTextures::Gray() { return s_Gray; }
    Ref<RHITexture> DefaultTextures::Blue() { return s_Blue; }
    Ref<RHITexture> DefaultTextures::BlackCube() { return s_BlackCube; }
    Ref<RHITexture> DefaultTextures::WhiteArray() { return s_WhiteArray; }
    Ref<RHITexture> DefaultTextures::BrdfLUT() { return s_BrdfLUT; }

}
