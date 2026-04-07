#pragma once
#include "Assets/Asset.h"
#include "Core/Buffer.h"
#include "RHI/RHI.h"

#include <imgui/imgui.h>
#include <vulkan/vulkan.h>

namespace Engine {

    // Keep TextureSpecification for compatibility with existing code (ModelImporter, etc.)
    struct TextureSpecification
    {
        uint32_t Width = 1;
        uint32_t Height = 1;
        uint32_t Size = 0;
        ImageFormat Format = ImageFormat::RGBA8;
        bool GenerateMips = true;
    };

    class Texture : public Asset
    {
    public:
        virtual ~Texture() = default;

        virtual const TextureSpecification& GetSpecification() const = 0;

        virtual uint32_t GetWidth() const = 0;
        virtual uint32_t GetHeight() const = 0;

        // Returns 0 for Vulkan. Kept for ImGui compatibility (ImGui Vulkan uses VkDescriptorSet, not texture IDs).
        virtual uint32_t GetRendererID() const { return 0; }

        // Returns an ImGui-compatible texture handle (VkDescriptorSet for Vulkan backend).
        virtual ImTextureID GetImGuiTextureID() { return nullptr; }

        virtual void SetData(Buffer data) = 0;

        virtual void Bind(uint32_t slot = 0) const {}

        virtual bool IsLoaded() const = 0;

        virtual bool operator==(const Texture& other) const = 0;

        virtual RHITexture* GetRHITexture() const = 0;
    };

    class Texture2D : public Texture {
    public:
        static Ref<Texture2D> Create(const TextureSpecification& specification, Buffer data);

        Texture2D(const TextureSpecification& specification, Buffer data);
        Texture2D(Ref<Texture2D> other);
        ~Texture2D();

        const TextureSpecification& GetSpecification() const override { return m_Specification; }

        uint32_t GetWidth() const override { return m_Specification.Width; }
        uint32_t GetHeight() const override { return m_Specification.Height; }
        uint32_t GetRendererID() const override { return 0; }

        ImTextureID GetImGuiTextureID() override;

        void SetData(Buffer data) override;

        void Bind(uint32_t slot = 0) const override {} // No-op for Vulkan

        bool IsLoaded() const override { return m_RHITexture != nullptr; }

        bool operator==(const Texture& other) const override
        {
            return GetRHITexture() == other.GetRHITexture();
        }

        RHITexture* GetRHITexture() const override { return m_RHITexture.get(); }

        static AssetType GetStaticType() { return AssetType::Texture; }
        AssetType GetType() const override { return GetStaticType(); }

    private:
        TextureSpecification m_Specification;
        Ref<RHITexture> m_RHITexture;
        VkDescriptorSet m_ImGuiDescriptor = VK_NULL_HANDLE;
    };


    class TextureCube : public Texture
    {
    public:
        static Ref<TextureCube> Create(Ref<Texture2D> map_texture, uint32_t size)
        {
            return CreateRef<TextureCube>(map_texture, size, ImageFormat::RGBA16F);
        }

        static Ref<TextureCube> Create(Ref<Texture2D> map_texture, uint32_t size, ImageFormat format)
        {
            return CreateRef<TextureCube>(map_texture, size, format);
        }

        TextureCube(Ref<Texture2D> map_texture, uint32_t size, ImageFormat format);
        ~TextureCube();

        const TextureSpecification& GetSpecification() const override { return m_Specification; }

        uint32_t GetWidth() const override { return m_Size; }
        uint32_t GetHeight() const override { return m_Size; }
        uint32_t GetRendererID() const override { return 0; }

        void Bind(uint32_t slot = 0) const override {} // No-op for Vulkan

        Ref<Texture2D> GetFlatTexture() { return m_FlattenedTexture; }

        void SetData(Buffer data) override;

        void GenerateMipMap() {} // TODO: implement with RHI

        bool IsLoaded() const override { return m_RHITexture != nullptr; }

        bool operator==(const Texture& other) const override
        {
            return GetRHITexture() == other.GetRHITexture();
        }

        RHITexture* GetRHITexture() const override { return m_RHITexture.get(); }

        static AssetType GetStaticType() { return AssetType::CubeMap; }
        AssetType GetType() const override { return GetStaticType(); }

    private:
        TextureSpecification m_Specification;
        uint32_t m_Size;
        Ref<RHITexture> m_RHITexture;
        Ref<Texture2D> m_FlattenedTexture;
    };

}
