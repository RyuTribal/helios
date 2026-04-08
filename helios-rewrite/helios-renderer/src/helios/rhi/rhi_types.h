#pragma once

#include <cstdint>
#include <string>
#include <type_traits>
#include <vector>

namespace helios::rhi {

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
    void* buffer_handle    = nullptr;  // native buffer pointer, cast by backend
    uint32_t offset        = 0;
    uint32_t range         = 0;        // 0 = whole buffer
    // Image fields (used when type is CombinedImageSampler or StorageImage)
    void* texture_handle   = nullptr;  // native texture pointer, cast by backend
};

// Forward declarations for backend types used in desc structs.
// The actual Vulkan types are defined in vulkan/ headers.
// GraphicsPipelineDesc and ComputePipelineDesc reference shader/layout/render-pass
// objects by pointer. In the new RAII design these are non-owning references
// to objects whose lifetime is managed by the caller.

struct GraphicsPipelineDesc {
    const void* vertex_shader     = nullptr;   // pointer to VulkanShader (or backend shader)
    const void* fragment_shader   = nullptr;
    const void* geometry_shader   = nullptr;
    VertexLayout layout;
    RenderState state;
    const void* render_pass       = nullptr;   // pointer to VulkanRenderPass (or backend RP)
    std::vector<const void*> descriptor_layouts; // pointers to descriptor set layouts
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
    const void* compute_shader    = nullptr;
    std::vector<const void*> descriptor_layouts;
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
