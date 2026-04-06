#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <glm/glm.hpp>

namespace Engine {

    // ---- Enums ----

    enum class ImageFormat {
        R8, RG8, RGB8, RGBA8,
        RG16F, RGBA16F, RGBA32F,
        R32F, RG32F, RGB32F,
        DEPTH24_STENCIL8, DEPTH32F
    };

    enum class TextureType { Texture2D, TextureCube, Texture2DArray };

    enum class TextureUsage : uint32_t {
        Sampled          = 1,
        ColorAttachment  = 2,
        DepthAttachment  = 4,
        Storage          = 8,
        Transfer         = 16
    };

    inline TextureUsage operator|(TextureUsage a, TextureUsage b) {
        return static_cast<TextureUsage>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
    }
    inline bool operator&(TextureUsage a, TextureUsage b) {
        return (static_cast<uint32_t>(a) & static_cast<uint32_t>(b)) != 0;
    }

    enum class BufferUsage : uint32_t {
        Vertex   = 1,
        Index    = 2,
        Uniform  = 4,
        Storage  = 8,
        Transfer = 16
    };

    inline BufferUsage operator|(BufferUsage a, BufferUsage b) {
        return static_cast<BufferUsage>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
    }
    inline bool operator&(BufferUsage a, BufferUsage b) {
        return (static_cast<uint32_t>(a) & static_cast<uint32_t>(b)) != 0;
    }

    enum class MemoryAccess { GPU_Only, CPU_to_GPU, GPU_to_CPU };

    enum class ShaderStage : uint32_t {
        Vertex   = 1,
        Fragment = 2,
        Compute  = 4,
        Geometry = 8
    };

    inline ShaderStage operator|(ShaderStage a, ShaderStage b) {
        return static_cast<ShaderStage>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
    }
    inline bool operator&(ShaderStage a, ShaderStage b) {
        return (static_cast<uint32_t>(a) & static_cast<uint32_t>(b)) != 0;
    }

    enum class IndexType { Uint16, Uint32 };
    enum class CullMode { None, Front, Back };
    enum class DepthCompare { Less, LessEqual, Greater, GreaterEqual, Never, Always };
    enum class BlendMode { None, Alpha, Additive };
    enum class LoadOp { Load, Clear, DontCare };
    enum class StoreOp { Store, DontCare };
    enum class DescriptorType { UniformBuffer, StorageBuffer, CombinedImageSampler, StorageImage };

    // ---- Description structs ----

    struct TextureDesc {
        uint32_t Width = 1, Height = 1;
        ImageFormat Format = ImageFormat::RGBA8;
        TextureType Type = TextureType::Texture2D;
        uint32_t MipLevels = 1;
        uint32_t ArrayLayers = 1;
        uint32_t Samples = 1;
        TextureUsage Usage = TextureUsage::Sampled;
        std::string DebugName;
    };

    struct BufferDesc {
        uint32_t Size = 0;
        BufferUsage Usage = BufferUsage::Vertex;
        MemoryAccess Access = MemoryAccess::GPU_Only;
        std::string DebugName;
    };

    struct ShaderDesc {
        ShaderStage Stage = ShaderStage::Vertex;
        std::vector<uint8_t> SpirVCode;
        std::string EntryPoint = "main";
        std::string DebugName;
    };

    struct VertexAttribute {
        uint32_t Location = 0;
        uint32_t Binding = 0;
        uint32_t Offset = 0;
        ImageFormat Format = ImageFormat::RGB32F;
    };

    struct VertexLayout {
        std::vector<VertexAttribute> Attributes;
        uint32_t Stride = 0;
    };

    struct RenderState {
        CullMode Cull = CullMode::Back;
        DepthCompare Depth = DepthCompare::Less;
        bool DepthWrite = true;
        bool DepthTest = true;
        BlendMode Blend = BlendMode::None;
    };

    struct AttachmentDesc {
        ImageFormat Format = ImageFormat::RGBA8;
        uint32_t Samples = 1;
        LoadOp Load = LoadOp::Clear;
        StoreOp Store = StoreOp::Store;
    };

    struct RenderPassDesc {
        std::vector<AttachmentDesc> ColorAttachments;
        AttachmentDesc DepthAttachment;
        bool HasDepth = false;
        std::string DebugName;
    };

    struct FramebufferDesc; // forward declare, defined in RHIResources.h

    struct DescriptorBinding {
        uint32_t Binding = 0;
        DescriptorType Type = DescriptorType::UniformBuffer;
        ShaderStage Stage = ShaderStage::Vertex;
        uint32_t Count = 1;
    };

    struct DescriptorSetLayoutDesc {
        std::vector<DescriptorBinding> Bindings;
        std::string DebugName;
    };

    struct ClearValues {
        glm::vec4 Color = {0.0f, 0.0f, 0.0f, 1.0f};
        float Depth = 1.0f;
        uint32_t Stencil = 0;
    };

    struct BarrierDesc {
        ShaderStage SrcStage = ShaderStage::Compute;
        ShaderStage DstStage = ShaderStage::Fragment;
    };

} // namespace Engine
