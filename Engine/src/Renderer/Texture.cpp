#include "pch.h"
#include "Texture.h"

// Forward declare to avoid circular include - Renderer::GetDevice() is used to get the RHI device
namespace Engine { class Renderer; }

namespace Engine {

    // Defined in Renderer.cpp - we use a free function to avoid circular header dependency
    extern RHIDevice* GetRendererDevice();

    namespace Utils {

        static uint32_t ImageFormatToChannels(ImageFormat format)
        {
            switch (format)
            {
                case ImageFormat::R8:       return 1;
                case ImageFormat::RG8:      return 2;
                case ImageFormat::RGB8:     return 3;
                case ImageFormat::RGBA8:    return 4;
                case ImageFormat::RG16F:    return 2;
                case ImageFormat::RGBA16F:  return 4;
                case ImageFormat::RG32F:    return 2;
                case ImageFormat::RGB32F:   return 3;
                case ImageFormat::RGBA32F:  return 4;
                default: break;
            }
            HVE_CORE_ASSERT(false, "Unknown image format for channel count");
            return 0;
        }

        static uint32_t ImageFormatBytesPerPixel(ImageFormat format)
        {
            switch (format)
            {
                case ImageFormat::R8:       return 1;
                case ImageFormat::RG8:      return 2;
                case ImageFormat::RGB8:     return 3;
                case ImageFormat::RGBA8:    return 4;
                case ImageFormat::RG16F:    return 4;
                case ImageFormat::RGBA16F:  return 8;
                case ImageFormat::RG32F:    return 8;
                case ImageFormat::RGB32F:   return 12;
                case ImageFormat::RGBA32F:  return 16;
                default: break;
            }
            return 4;
        }
    }

    // ---- Texture2D ----

    Ref<Texture2D> Texture2D::Create(const TextureSpecification& specification, Buffer data)
    {
        return CreateRef<Texture2D>(specification, data);
    }

    Texture2D::Texture2D(const TextureSpecification& specification, Buffer data)
        : m_Specification(specification)
    {
        RHIDevice* device = GetRendererDevice();
        if (!device)
        {
            HVE_CORE_ERROR_TAG("Texture", "Texture2D::Create called before Renderer is initialized");
            return;
        }

        TextureDesc desc;
        desc.Width = m_Specification.Width;
        desc.Height = m_Specification.Height;
        desc.Format = m_Specification.Format;
        desc.Type = TextureType::Texture2D;
        desc.MipLevels = m_Specification.GenerateMips ? 1 : 1; // TODO: calculate mip count
        desc.Usage = TextureUsage::Sampled | TextureUsage::Transfer;
        desc.DebugName = "Texture2D";

        const void* initialData = data ? data.Data : nullptr;
        m_RHITexture = device->CreateTexture(desc, initialData);
    }

    Texture2D::Texture2D(Ref<Texture2D> other)
        : m_Specification(other->GetSpecification())
    {
        // For Vulkan we just share the texture reference for now
        // A proper copy would require a blit command
        m_RHITexture = other->m_RHITexture;
    }

    Texture2D::~Texture2D()
    {
        // Ref<RHITexture> will clean up automatically
    }

    void Texture2D::SetData(Buffer data)
    {
        if (!m_RHITexture)
            return;

        // Re-create the texture with new data
        RHIDevice* device = GetRendererDevice();
        if (!device)
            return;

        TextureDesc desc;
        desc.Width = m_Specification.Width;
        desc.Height = m_Specification.Height;
        desc.Format = m_Specification.Format;
        desc.Type = TextureType::Texture2D;
        desc.MipLevels = 1;
        desc.Usage = TextureUsage::Sampled | TextureUsage::Transfer;
        desc.DebugName = "Texture2D";

        m_RHITexture = device->CreateTexture(desc, data.Data);
    }


    // ---- TextureCube ----

    TextureCube::TextureCube(Ref<Texture2D> map_texture, uint32_t size, ImageFormat format)
        : m_FlattenedTexture(map_texture), m_Size(size)
    {
        m_Specification.Width = size;
        m_Specification.Height = size;
        m_Specification.Format = format;

        RHIDevice* device = GetRendererDevice();
        if (!device)
        {
            HVE_CORE_ERROR_TAG("Texture", "TextureCube::Create called before Renderer is initialized");
            return;
        }

        TextureDesc desc;
        desc.Width = size;
        desc.Height = size;
        desc.Format = format;
        desc.Type = TextureType::TextureCube;
        desc.MipLevels = 1;
        desc.ArrayLayers = 6;
        desc.Usage = TextureUsage::Sampled | TextureUsage::Transfer;
        desc.DebugName = "TextureCube";

        m_RHITexture = device->CreateTexture(desc);
    }

    TextureCube::~TextureCube()
    {
    }

    void TextureCube::SetData(Buffer data)
    {
        // TODO: upload cubemap face data through RHI
        if (m_FlattenedTexture)
            m_FlattenedTexture->SetData(data);
    }

}
