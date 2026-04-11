#pragma once

#include <cstdint>
#include <string>
#include <type_traits>
#include <vector>

namespace helios::rhi {

// Forward declarations for abstract base types referenced by descriptor structs.
class Shader;
class Buffer;
class Texture;
class RenderPass;
class DescriptorSetLayout;

// ---------------------------------------------------------------------------
//  Bitfield operator helper macro
// ---------------------------------------------------------------------------
#define HELIOS_ENUM_FLAGS(EnumType)                                           \
    inline constexpr EnumType operator|(EnumType a, EnumType b) {             \
        using U = std::underlying_type_t<EnumType>;                           \
        return static_cast<EnumType>(static_cast<U>(a) | static_cast<U>(b)); \
    }                                                                         \
    inline constexpr EnumType operator&(EnumType a, EnumType b) {             \
        using U = std::underlying_type_t<EnumType>;                           \
        return static_cast<EnumType>(static_cast<U>(a) & static_cast<U>(b)); \
    }                                                                         \
    inline constexpr EnumType operator~(EnumType a) {                         \
        using U = std::underlying_type_t<EnumType>;                           \
        return static_cast<EnumType>(~static_cast<U>(a));                     \
    }                                                                         \
    inline constexpr EnumType& operator|=(EnumType& a, EnumType b) {          \
        return a = a | b;                                                     \
    }                                                                         \
    inline constexpr EnumType& operator&=(EnumType& a, EnumType b) {          \
        return a = a & b;                                                     \
    }                                                                         \
    inline constexpr bool has_flag(EnumType flags, EnumType bit) {            \
        using U = std::underlying_type_t<EnumType>;                           \
        return (static_cast<U>(flags) & static_cast<U>(bit)) != 0;           \
    }

// ---------------------------------------------------------------------------
//  Enums
// ---------------------------------------------------------------------------

enum class TextureFormat : uint8_t {
    R8,
    RG8,
    RGBA8,
    BGRA8,          // common swapchain format (B8G8R8A8_UNORM)
    RG16F,
    RGBA16F,
    R32F,
    RG32F,
    RGB32F,
    RGBA32F,
    Depth32F,
    Depth24Stencil8,
};

enum class TextureType : uint8_t {
    Texture2D,
    TextureCube,
    Texture2DArray,
};

enum class BufferUsage : uint32_t {
    Vertex   = 1 << 0,
    Index    = 1 << 1,
    Uniform  = 1 << 2,
    Storage  = 1 << 3,
    Transfer = 1 << 4,
};
HELIOS_ENUM_FLAGS(BufferUsage)

enum class TextureUsage : uint32_t {
    Sampled         = 1 << 0,
    Storage         = 1 << 1,
    ColorAttachment = 1 << 2,
    DepthAttachment = 1 << 3,
    Transfer        = 1 << 4,
};
HELIOS_ENUM_FLAGS(TextureUsage)

enum class ShaderStage : uint32_t {
    Vertex   = 1 << 0,
    Fragment = 1 << 1,
    Compute  = 1 << 2,
    Geometry = 1 << 3,
};
HELIOS_ENUM_FLAGS(ShaderStage)

enum class CullMode : uint8_t {
    None,
    Front,
    Back,
};

enum class DepthCompare : uint8_t {
    Less,
    LessEqual,
    Greater,
    GreaterEqual,
    Never,
    Always,
};

enum class BlendMode : uint8_t {
    None,
    Alpha,
    Additive,
};

enum class LoadOp : uint8_t {
    Load,
    Clear,
    DontCare,
};

enum class StoreOp : uint8_t {
    Store,
    DontCare,
};

enum class PresentMode : uint8_t {
    Immediate,   // no vsync, tearing possible
    Fifo,        // vsync, guaranteed available
    Mailbox,     // vsync with late-swap (triple-buffer feel)
};

enum class SamplerMode : uint8_t {
    Repeat,
    ClampToEdge,
    MirroredRepeat,
};

enum class MemoryAccess : uint8_t {
    GPU_Only,
    CPU_to_GPU,
    GPU_to_CPU,
};

enum class IndexType : uint8_t {
    Uint16,
    Uint32,
};

enum class DescriptorType : uint8_t {
    UniformBuffer,
    StorageBuffer,
    CombinedImageSampler,
    StorageImage,
};

// ---------------------------------------------------------------------------
//  Descriptor structs
// ---------------------------------------------------------------------------

struct TextureDesc {
    uint32_t width         = 1;
    uint32_t height        = 1;
    TextureFormat format   = TextureFormat::RGBA8;
    TextureType type       = TextureType::Texture2D;
    uint32_t mip_levels    = 1;
    uint32_t array_layers  = 1;
    TextureUsage usage     = TextureUsage::Sampled;
    SamplerMode sampler    = SamplerMode::Repeat;
    std::string debug_name;
};

struct BufferDesc {
    uint32_t size          = 0;
    BufferUsage usage      = BufferUsage::Vertex;
    MemoryAccess access    = MemoryAccess::GPU_Only;
    std::string debug_name;
};

struct ShaderDesc {
    ShaderStage stage          = ShaderStage::Vertex;
    std::vector<uint8_t> spirv_code;
    std::string entry_point    = "main";
    std::string debug_name;
};

struct VertexAttribute {
    uint32_t location      = 0;
    uint32_t binding       = 0;
    uint32_t offset        = 0;
    TextureFormat format   = TextureFormat::RGB32F;
};

struct VertexLayout {
    std::vector<VertexAttribute> attributes;
    uint32_t stride        = 0;
};

struct RenderState {
    CullMode cull          = CullMode::Back;
    DepthCompare depth     = DepthCompare::Less;
    bool depth_write       = true;
    bool depth_test        = true;
    BlendMode blend        = BlendMode::None;
};

struct AttachmentDesc {
    TextureFormat format   = TextureFormat::RGBA8;
    uint32_t samples       = 1;
    LoadOp load            = LoadOp::Clear;
    StoreOp store          = StoreOp::Store;
};

struct RenderPassDesc {
    std::vector<AttachmentDesc> color_attachments;
    AttachmentDesc depth_attachment;
    bool has_depth         = false;
    std::string debug_name;
};

struct FramebufferDesc {
    uint32_t width         = 0;
    uint32_t height        = 0;
    std::string debug_name;
    // Attachment image views are provided by the backend types directly.
    // This is populated at construction time with native handle references.
};

struct DescriptorBinding {
    uint32_t binding       = 0;
    DescriptorType type    = DescriptorType::UniformBuffer;
    ShaderStage stage      = ShaderStage::Vertex;
    uint32_t count         = 1;
};

struct DescriptorSetLayoutDesc {
    std::vector<DescriptorBinding> bindings;
    std::string debug_name;
};

struct DescriptorWrite {
    uint32_t binding       = 0;
    DescriptorType type    = DescriptorType::UniformBuffer;
    // Buffer fields (used when type is UniformBuffer or StorageBuffer)
    Buffer* buffer_handle  = nullptr;
    uint32_t offset        = 0;
    uint32_t range         = 0;        // 0 = whole buffer
    // Image fields (used when type is CombinedImageSampler or StorageImage)
    Texture* texture_handle = nullptr;
};

struct GraphicsPipelineDesc {
    const Shader* vertex_shader   = nullptr;
    const Shader* fragment_shader = nullptr;
    const Shader* geometry_shader = nullptr;
    VertexLayout layout;
    RenderState state;
    const RenderPass* render_pass = nullptr;
    std::vector<const DescriptorSetLayout*> descriptor_layouts;
    uint32_t push_constant_size   = 0;
    ShaderStage push_constant_stages = ShaderStage::Vertex;
    std::string debug_name;
    // Dynamic rendering: when render_pass is nullptr, these formats are used
    // to create the pipeline via VkPipelineRenderingCreateInfo (Vulkan 1.3).
    std::vector<TextureFormat> dynamic_color_formats;
    TextureFormat dynamic_depth_format = TextureFormat::Depth32F;
    bool use_dynamic_rendering = false;
};

struct ComputePipelineDesc {
    const Shader* compute_shader  = nullptr;
    std::vector<const DescriptorSetLayout*> descriptor_layouts;
    uint32_t push_constant_size   = 0;
    std::string debug_name;
};

struct SwapchainDesc {
    uint32_t width         = 0;
    uint32_t height        = 0;
    void* surface          = nullptr;  // VkSurfaceKHR, cast by backend
    PresentMode present_mode = PresentMode::Fifo;
};

struct ClearValues {
    float color[4]         = {0.0f, 0.0f, 0.0f, 1.0f};
    float depth            = 1.0f;
    uint32_t stencil       = 0;
};

/// Image/texture layout for transitions. Backend-agnostic equivalent of VkImageLayout.
enum class TextureLayout : uint8_t {
    Undefined,
    General,
    ColorAttachment,
    DepthAttachment,
    ShaderReadOnly,
    TransferSrc,
    TransferDst,
    PresentSrc,
};

struct BarrierDesc {
    ShaderStage src_stage  = ShaderStage::Compute;
    ShaderStage dst_stage  = ShaderStage::Fragment;
};

struct SubmitInfo {
    void* wait_semaphore       = nullptr;  // VkSemaphore to wait on before execution
    void* signal_semaphore     = nullptr;  // VkSemaphore to signal after execution
    void* fence                = nullptr;  // VkFence to signal after execution
};

} // namespace helios::rhi
