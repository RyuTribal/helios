#include "pch.h"
#include "DefaultTextures.h"

namespace Engine {

    Ref<RHITexture> DefaultTextures::s_White;
    Ref<RHITexture> DefaultTextures::s_Black;
    Ref<RHITexture> DefaultTextures::s_Gray;
    Ref<RHITexture> DefaultTextures::s_Blue;

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

        HVE_CORE_INFO_TAG("DefaultTextures", "Default textures initialized");
    }

    void DefaultTextures::Shutdown()
    {
        s_White.reset();
        s_Black.reset();
        s_Gray.reset();
        s_Blue.reset();
    }

    Ref<RHITexture> DefaultTextures::White() { return s_White; }
    Ref<RHITexture> DefaultTextures::Black() { return s_Black; }
    Ref<RHITexture> DefaultTextures::Gray() { return s_Gray; }
    Ref<RHITexture> DefaultTextures::Blue() { return s_Blue; }

}
