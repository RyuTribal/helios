# Plan 4: RHI (Rendering Hardware Interface) + Vulkan Backend

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Create the `helios-renderer` library with a compile-time-selected Rendering Hardware Interface (RHI) and a complete Vulkan 1.3 backend. All GPU resource types are RAII move-only value types. No `Init()`/`Shutdown()` methods -- constructor creates, destructor destroys.

**Depends on:** Plan 1 (ECS types). Assumes `helios-core/` exists with World, Entity, App, Plugin, logging, etc.

**Architecture:** Thin Forge-style RHI abstraction. Compile-time backend selection via `#ifdef HELIOS_BACKEND_VULKAN` -- no virtual dispatch for RHI calls. Vulkan backend adapted from existing `Engine/src/Renderer/Vulkan/` code, refactored into RAII move-only types.

**Tech Stack:** Vulkan 1.3+, VMA (Vulkan Memory Allocator), vk-bootstrap, GLFW (surface), C++20

**Spec reference:** `docs/superpowers/specs/2026-04-07-engine-rewrite-design.md` section 4.1 (RHI)

**Existing code reference:** `Engine/src/Renderer/Vulkan/` (adapt patterns, do not copy verbatim)

**Key design rules (from spec):**
- **RAII everywhere.** Constructor = create GPU resource. Destructor = destroy GPU resource. No `Init()`/`Shutdown()`.
- **Move-only.** All RHI types: delete copy ctor/assignment, implement move ctor/assignment. Move-assign frees the old resource before taking ownership of the new one.
- **Prefer std library.** `std::vector`, `std::string`, `std::function`, `std::optional`. No custom `Ref<T>`/`Scope<T>`.
- **No global state.** `VulkanContext` becomes a proper RAII object, not a static singleton.
- **Compile-time backend.** `using Device = vulkan::VulkanDevice;` -- not virtual interfaces.
- **Vulkan 1.3 modern practices.** Dynamic rendering (`vkCmdBeginRendering`), synchronization2 (`vkCmdPipelineBarrier2`), validation layers with debug object naming.

---

### Task 1: Create `helios-renderer/` CMake target

**Files:**
- Create: `helios-renderer/CMakeLists.txt`
- Modify: Root `CMakeLists.txt` (add `add_subdirectory(helios-renderer)` -- or create root CMakeLists.txt if not yet present)

- [ ] **Step 1: Create the directory structure**

```bash
mkdir -p helios-renderer/src/rhi
mkdir -p helios-renderer/src/vulkan
```

- [ ] **Step 2: Create `helios-renderer/CMakeLists.txt`**

```cmake
cmake_minimum_required(VERSION 3.20)

option(HELIOS_BACKEND_VULKAN "Use Vulkan backend" ON)

find_package(Vulkan REQUIRED)

file(GLOB_RECURSE HELIOS_RENDERER_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
)

add_library(helios-renderer STATIC ${HELIOS_RENDERER_SOURCES})

target_include_directories(helios-renderer
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    PRIVATE
        ${Vulkan_INCLUDE_DIRS}
)

target_link_libraries(helios-renderer
    PUBLIC
        helios-core
    PRIVATE
        Vulkan::Vulkan
)

# VMA: header-only, include path to vendored header
target_include_directories(helios-renderer PRIVATE
    ${CMAKE_SOURCE_DIR}/vendor/vma
)

# vk-bootstrap: vendored source
target_include_directories(helios-renderer PRIVATE
    ${CMAKE_SOURCE_DIR}/vendor/vk-bootstrap
)
# vk-bootstrap .cpp compiled as part of helios-renderer sources or as separate object;
# if separate, add it here:
target_sources(helios-renderer PRIVATE
    ${CMAKE_SOURCE_DIR}/vendor/vk-bootstrap/VkBootstrap.cpp
)

target_compile_definitions(helios-renderer
    PUBLIC
        $<$<BOOL:${HELIOS_BACKEND_VULKAN}>:HELIOS_BACKEND_VULKAN>
)

target_compile_features(helios-renderer PUBLIC cxx_std_20)

# Debug: enable validation layer defines
target_compile_definitions(helios-renderer PRIVATE
    $<$<CONFIG:Debug>:HELIOS_VULKAN_VALIDATION=1>
    $<$<CONFIG:RelWithDebInfo>:HELIOS_VULKAN_VALIDATION=1>
)
```

- [ ] **Step 3: Create VMA implementation file**

Create `helios-renderer/src/vulkan/vulkan_vma.cpp`:

```cpp
// Single compilation unit for VMA implementation.
// VMA is header-only; this file provides the one required definition.

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>
```

- [ ] **Step 4: Verify the build scaffolding compiles (empty library)**

```bash
# From repo root, after root CMakeLists.txt includes helios-renderer:
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target helios-renderer
```

---

### Task 2: RHI enums and bitfield operators (`rhi/rhi_types.h`)

**Files:**
- Create: `helios-renderer/src/rhi/rhi_types.h`

- [ ] **Step 1: Create `helios-renderer/src/rhi/rhi_types.h` with all enums and bitfield operators**

```cpp
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
```

- [ ] **Step 2: Verify the header compiles**

```bash
cmake --build build --target helios-renderer
```

---

### Task 3: Compile-time backend selection (`rhi/rhi.h`)

**Files:**
- Create: `helios-renderer/src/rhi/rhi.h`

- [ ] **Step 1: Create `helios-renderer/src/rhi/rhi.h`**

```cpp
#pragma once

// Compile-time backend selection.
// Only one backend is compiled at a time. The using-declarations below
// make backend types available as helios::rhi::Device, helios::rhi::Texture, etc.
// No virtual dispatch overhead -- callers use concrete types directly.

#include "rhi/rhi_types.h"

#if defined(HELIOS_BACKEND_VULKAN)
    #include "vulkan/vulkan_context.h"
    #include "vulkan/vulkan_device.h"
    #include "vulkan/vulkan_texture.h"
    #include "vulkan/vulkan_buffer.h"
    #include "vulkan/vulkan_pipeline.h"
    #include "vulkan/vulkan_command_buffer.h"
    #include "vulkan/vulkan_swapchain.h"
    #include "vulkan/vulkan_descriptor.h"
    #include "vulkan/vulkan_render_pass.h"
    #include "vulkan/vulkan_framebuffer.h"
    #include "vulkan/vulkan_shader.h"

    namespace helios::rhi {
        using Context         = vulkan::VulkanContext;
        using Device          = vulkan::VulkanDevice;
        using Texture         = vulkan::VulkanTexture;
        using Buffer          = vulkan::VulkanBuffer;
        using Pipeline        = vulkan::VulkanPipeline;
        using CommandBuffer   = vulkan::VulkanCommandBuffer;
        using Swapchain       = vulkan::VulkanSwapchain;
        using DescriptorSet   = vulkan::VulkanDescriptorSet;
        using DescriptorSetLayout = vulkan::VulkanDescriptorSetLayout;
        using RenderPass      = vulkan::VulkanRenderPass;
        using Framebuffer     = vulkan::VulkanFramebuffer;
        using Shader          = vulkan::VulkanShader;
    }
#else
    #error "No rendering backend selected. Define HELIOS_BACKEND_VULKAN."
#endif
```

- [ ] **Step 2: This file will not compile yet (backend headers do not exist). Verify syntax only -- full build tested after all backend types are implemented.**

---

### Task 4: `VulkanContext` -- Vulkan instance + validation layers (RAII)

**Files:**
- Create: `helios-renderer/src/vulkan/vulkan_context.h`
- Create: `helios-renderer/src/vulkan/vulkan_context.cpp`

Adapt from: `Engine/src/Renderer/Vulkan/VulkanContext.h/.cpp`

Key changes from old code: Remove all `static` members and `Init()`/`Shutdown()`. Constructor creates VkInstance via vk-bootstrap. Destructor destroys it. No global state.

- [ ] **Step 1: Create `helios-renderer/src/vulkan/vulkan_context.h`**

```cpp
#pragma once

#include <vulkan/vulkan.h>
#include <VkBootstrap.h>
#include <string>

struct GLFWwindow;

namespace helios::rhi::vulkan {

// RAII Vulkan instance wrapper.
// Constructor creates VkInstance + debug messenger (if validation enabled).
// Destructor destroys both. Move-only.
class VulkanContext {
public:
    VulkanContext(const char* app_name, bool enable_validation);
    ~VulkanContext();

    VulkanContext(VulkanContext&& other) noexcept;
    VulkanContext& operator=(VulkanContext&& other) noexcept;
    VulkanContext(const VulkanContext&) = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;

    VkInstance instance() const { return m_instance.instance; }
    const vkb::Instance& vkb_instance() const { return m_instance; }
    bool validation_enabled() const { return m_validation_enabled; }

    // Surface creation/destruction (wraps glfwCreateWindowSurface)
    VkSurfaceKHR create_surface(GLFWwindow* window) const;
    void destroy_surface(VkSurfaceKHR surface) const;

    // Debug object naming -- call on every Vulkan object for better validation messages
    void set_debug_name(VkDevice device, VkObjectType type,
                        uint64_t handle, const char* name) const;

    explicit operator bool() const { return m_instance.instance != VK_NULL_HANDLE; }

private:
    void destroy();

    vkb::Instance m_instance{};
    bool m_validation_enabled = false;
};

} // namespace helios::rhi::vulkan
```

- [ ] **Step 2: Create `helios-renderer/src/vulkan/vulkan_context.cpp`**

Adapt the implementation from `Engine/src/Renderer/Vulkan/VulkanContext.cpp`. Key changes:

1. Replace `static bool Init(...)` body with constructor body.
2. Replace `static void Shutdown()` body with destructor body.
3. Replace `static` member access (`s_Instance`, `s_ValidationEnabled`) with `m_instance`, `m_validation_enabled`.
4. Implement move constructor/assignment (swap all members, leave source in null state).
5. Replace `HVE_CORE_*_TAG` with `spdlog` calls or keep the same logging macro if helios-core provides it. Use a forward-compatible logging call: `HELIOS_LOG_INFO("Vulkan", ...)` or `spdlog::info(...)`.

```cpp
#include "vulkan/vulkan_context.h"

#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>
#include <utility>

namespace helios::rhi::vulkan {

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT /*type*/,
    const VkDebugUtilsMessengerCallbackDataEXT* data,
    void* /*user_data*/)
{
    const char* id = data->pMessageIdName ? data->pMessageIdName : "Unknown";
    switch (severity) {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
            spdlog::trace("[Vulkan] [{}] {}", id, data->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
            spdlog::info("[Vulkan] [{}] {}", id, data->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
            spdlog::warn("[Vulkan] [{}] {}", id, data->pMessage);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
            spdlog::error("[Vulkan] [{}] {}", id, data->pMessage);
            break;
        default:
            spdlog::trace("[Vulkan] [{}] {}", id, data->pMessage);
            break;
    }
    return VK_FALSE;
}

VulkanContext::VulkanContext(const char* app_name, bool enable_validation)
    : m_validation_enabled(enable_validation)
{
    vkb::InstanceBuilder builder;
    builder.set_app_name(app_name)
           .set_engine_name("Helios")
           .require_api_version(1, 3, 0);

    if (enable_validation) {
        builder.request_validation_layers()
               .set_debug_callback(debug_callback)
               .set_debug_messenger_severity(
                   VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                   VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                   VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                   VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
               .set_debug_messenger_type(
                   VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                   VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                   VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT);
    }

    auto result = builder.build();
    if (!result) {
        spdlog::error("[Vulkan] Failed to create Vulkan instance: {}",
                      result.error().message());
        return;
    }

    m_instance = result.value();
    spdlog::info("[Vulkan] Vulkan instance created (validation {})",
                 enable_validation ? "enabled" : "disabled");
}

VulkanContext::~VulkanContext()
{
    destroy();
}

VulkanContext::VulkanContext(VulkanContext&& other) noexcept
    : m_instance(std::exchange(other.m_instance, vkb::Instance{}))
    , m_validation_enabled(std::exchange(other.m_validation_enabled, false))
{
}

VulkanContext& VulkanContext::operator=(VulkanContext&& other) noexcept
{
    if (this != &other) {
        destroy();
        m_instance = std::exchange(other.m_instance, vkb::Instance{});
        m_validation_enabled = std::exchange(other.m_validation_enabled, false);
    }
    return *this;
}

void VulkanContext::destroy()
{
    if (m_instance.instance != VK_NULL_HANDLE) {
        vkb::destroy_debug_utils_messenger(m_instance.instance, m_instance.debug_messenger);
        vkb::destroy_instance(m_instance);
        m_instance = {};
        spdlog::info("[Vulkan] Vulkan instance destroyed");
    }
}

VkSurfaceKHR VulkanContext::create_surface(GLFWwindow* window) const
{
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkResult result = glfwCreateWindowSurface(m_instance.instance, window, nullptr, &surface);
    if (result != VK_SUCCESS) {
        spdlog::error("[Vulkan] Failed to create window surface (VkResult: {})",
                      static_cast<int>(result));
        return VK_NULL_HANDLE;
    }
    spdlog::info("[Vulkan] Window surface created");
    return surface;
}

void VulkanContext::destroy_surface(VkSurfaceKHR surface) const
{
    if (surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(m_instance.instance, surface, nullptr);
    }
}

void VulkanContext::set_debug_name(VkDevice device, VkObjectType type,
                                   uint64_t handle, const char* name) const
{
    if (!m_validation_enabled) return;

    VkDebugUtilsObjectNameInfoEXT info{};
    info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    info.objectType = type;
    info.objectHandle = handle;
    info.pObjectName = name;

    auto func = reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(
        vkGetDeviceProcAddr(device, "vkSetDebugUtilsObjectNameEXT"));
    if (func) func(device, &info);
}

} // namespace helios::rhi::vulkan
```

- [ ] **Step 3: Verify compilation of VulkanContext in isolation**

---

### Task 5: Vulkan format/enum conversion utilities (`vulkan/vulkan_utils.h`)

**Files:**
- Create: `helios-renderer/src/vulkan/vulkan_utils.h`

Adapt from: `Engine/src/Renderer/Vulkan/VulkanUtils.h`

- [ ] **Step 1: Create `helios-renderer/src/vulkan/vulkan_utils.h`**

This header provides inline conversion functions from RHI enums to Vulkan enums. All functions are `constexpr` or `inline` in a header -- no .cpp needed.

```cpp
#pragma once

#include "rhi/rhi_types.h"
#include <vulkan/vulkan.h>

namespace helios::rhi::vulkan {

inline VkFormat to_vk_format(TextureFormat format)
{
    switch (format) {
        case TextureFormat::R8:              return VK_FORMAT_R8_UNORM;
        case TextureFormat::RG8:             return VK_FORMAT_R8G8_UNORM;
        case TextureFormat::RGBA8:           return VK_FORMAT_R8G8B8A8_UNORM;
        case TextureFormat::RG16F:           return VK_FORMAT_R16G16_SFLOAT;
        case TextureFormat::RGBA16F:         return VK_FORMAT_R16G16B16A16_SFLOAT;
        case TextureFormat::R32F:            return VK_FORMAT_R32_SFLOAT;
        case TextureFormat::RG32F:           return VK_FORMAT_R32G32_SFLOAT;
        case TextureFormat::RGB32F:          return VK_FORMAT_R32G32B32_SFLOAT;
        case TextureFormat::RGBA32F:         return VK_FORMAT_R32G32B32A32_SFLOAT;
        case TextureFormat::Depth32F:        return VK_FORMAT_D32_SFLOAT;
        case TextureFormat::Depth24Stencil8: return VK_FORMAT_D24_UNORM_S8_UINT;
    }
    return VK_FORMAT_UNDEFINED;
}

inline VkAttachmentLoadOp to_vk_load_op(LoadOp op)
{
    switch (op) {
        case LoadOp::Load:     return VK_ATTACHMENT_LOAD_OP_LOAD;
        case LoadOp::Clear:    return VK_ATTACHMENT_LOAD_OP_CLEAR;
        case LoadOp::DontCare: return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    }
    return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
}

inline VkAttachmentStoreOp to_vk_store_op(StoreOp op)
{
    switch (op) {
        case StoreOp::Store:    return VK_ATTACHMENT_STORE_OP_STORE;
        case StoreOp::DontCare: return VK_ATTACHMENT_STORE_OP_DONT_CARE;
    }
    return VK_ATTACHMENT_STORE_OP_DONT_CARE;
}

inline VkShaderStageFlags to_vk_shader_stage_flags(ShaderStage stage)
{
    VkShaderStageFlags flags = 0;
    if (has_flag(stage, ShaderStage::Vertex))   flags |= VK_SHADER_STAGE_VERTEX_BIT;
    if (has_flag(stage, ShaderStage::Fragment))  flags |= VK_SHADER_STAGE_FRAGMENT_BIT;
    if (has_flag(stage, ShaderStage::Compute))   flags |= VK_SHADER_STAGE_COMPUTE_BIT;
    if (has_flag(stage, ShaderStage::Geometry))  flags |= VK_SHADER_STAGE_GEOMETRY_BIT;
    return flags;
}

inline VkPipelineStageFlags2 to_vk_pipeline_stage2(ShaderStage stage)
{
    VkPipelineStageFlags2 flags = 0;
    if (has_flag(stage, ShaderStage::Vertex))   flags |= VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
    if (has_flag(stage, ShaderStage::Fragment))  flags |= VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    if (has_flag(stage, ShaderStage::Compute))   flags |= VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    if (has_flag(stage, ShaderStage::Geometry))  flags |= VK_PIPELINE_STAGE_2_GEOMETRY_SHADER_BIT;
    return flags;
}

inline VkCullModeFlags to_vk_cull_mode(CullMode mode)
{
    switch (mode) {
        case CullMode::None:  return VK_CULL_MODE_NONE;
        case CullMode::Front: return VK_CULL_MODE_FRONT_BIT;
        case CullMode::Back:  return VK_CULL_MODE_BACK_BIT;
    }
    return VK_CULL_MODE_NONE;
}

inline VkCompareOp to_vk_compare_op(DepthCompare compare)
{
    switch (compare) {
        case DepthCompare::Less:         return VK_COMPARE_OP_LESS;
        case DepthCompare::LessEqual:    return VK_COMPARE_OP_LESS_OR_EQUAL;
        case DepthCompare::Greater:      return VK_COMPARE_OP_GREATER;
        case DepthCompare::GreaterEqual: return VK_COMPARE_OP_GREATER_OR_EQUAL;
        case DepthCompare::Never:        return VK_COMPARE_OP_NEVER;
        case DepthCompare::Always:       return VK_COMPARE_OP_ALWAYS;
    }
    return VK_COMPARE_OP_LESS;
}

inline VkPresentModeKHR to_vk_present_mode(PresentMode mode)
{
    switch (mode) {
        case PresentMode::Immediate: return VK_PRESENT_MODE_IMMEDIATE_KHR;
        case PresentMode::Fifo:      return VK_PRESENT_MODE_FIFO_KHR;
        case PresentMode::Mailbox:   return VK_PRESENT_MODE_MAILBOX_KHR;
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

inline VkSamplerAddressMode to_vk_sampler_mode(SamplerMode mode)
{
    switch (mode) {
        case SamplerMode::Repeat:         return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        case SamplerMode::ClampToEdge:    return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        case SamplerMode::MirroredRepeat: return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    }
    return VK_SAMPLER_ADDRESS_MODE_REPEAT;
}

inline VkDescriptorType to_vk_descriptor_type(DescriptorType type)
{
    switch (type) {
        case DescriptorType::UniformBuffer:        return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        case DescriptorType::StorageBuffer:        return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        case DescriptorType::CombinedImageSampler: return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        case DescriptorType::StorageImage:         return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    }
    return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
}

inline bool is_depth_format(TextureFormat format)
{
    return format == TextureFormat::Depth32F || format == TextureFormat::Depth24Stencil8;
}

inline uint32_t format_bytes_per_pixel(TextureFormat format)
{
    switch (format) {
        case TextureFormat::R8:              return 1;
        case TextureFormat::RG8:             return 2;
        case TextureFormat::RGBA8:           return 4;
        case TextureFormat::RG16F:           return 4;
        case TextureFormat::RGBA16F:         return 8;
        case TextureFormat::R32F:            return 4;
        case TextureFormat::RG32F:           return 8;
        case TextureFormat::RGB32F:          return 12;
        case TextureFormat::RGBA32F:         return 16;
        case TextureFormat::Depth32F:        return 4;
        case TextureFormat::Depth24Stencil8: return 4;
    }
    return 4;
}

inline VkImageViewType to_vk_view_type(TextureType type)
{
    switch (type) {
        case TextureType::Texture2D:      return VK_IMAGE_VIEW_TYPE_2D;
        case TextureType::TextureCube:    return VK_IMAGE_VIEW_TYPE_CUBE;
        case TextureType::Texture2DArray: return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    }
    return VK_IMAGE_VIEW_TYPE_2D;
}

} // namespace helios::rhi::vulkan
```

---

### Task 6: `VulkanDevice` -- physical/logical device, VMA, queues, ImmediateSubmit

**Files:**
- Create: `helios-renderer/src/vulkan/vulkan_device.h`
- Create: `helios-renderer/src/vulkan/vulkan_device.cpp`

Adapt from: `Engine/src/Renderer/Vulkan/VulkanDevice.h/.cpp`

Key changes: No inheritance from `RHIDevice`. No `Ref<>` return types. Factory methods return RAII value types by value (move semantics). Constructor takes `VulkanContext&` + `VkSurfaceKHR` instead of just `VkSurfaceKHR`. Stores reference to context for debug naming. Move-only.

- [ ] **Step 1: Create `helios-renderer/src/vulkan/vulkan_device.h`**

```cpp
#pragma once

#include "rhi/rhi_types.h"
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <functional>

namespace helios::rhi::vulkan {

class VulkanContext;
class VulkanTexture;
class VulkanBuffer;
class VulkanPipeline;
class VulkanCommandBuffer;
class VulkanSwapchain;
class VulkanDescriptorSet;
class VulkanDescriptorSetLayout;
class VulkanRenderPass;
class VulkanFramebuffer;
class VulkanShader;

class VulkanDevice {
public:
    VulkanDevice(VulkanContext& context, VkSurfaceKHR surface);
    ~VulkanDevice();

    VulkanDevice(VulkanDevice&& other) noexcept;
    VulkanDevice& operator=(VulkanDevice&& other) noexcept;
    VulkanDevice(const VulkanDevice&) = delete;
    VulkanDevice& operator=(const VulkanDevice&) = delete;

    // Factory methods -- return RAII value types by value (moved out)
    VulkanTexture create_texture(const TextureDesc& desc, const void* data = nullptr);
    VulkanBuffer create_buffer(const BufferDesc& desc, const void* data = nullptr);
    VulkanPipeline create_graphics_pipeline(const GraphicsPipelineDesc& desc);
    VulkanPipeline create_compute_pipeline(const ComputePipelineDesc& desc);
    VulkanCommandBuffer create_command_buffer();
    VulkanSwapchain create_swapchain(const SwapchainDesc& desc);
    VulkanDescriptorSetLayout create_descriptor_set_layout(const DescriptorSetLayoutDesc& desc);
    VulkanDescriptorSet create_descriptor_set(const VulkanDescriptorSetLayout& layout);
    VulkanRenderPass create_render_pass(const RenderPassDesc& desc);
    VulkanFramebuffer create_framebuffer(const FramebufferDesc& desc);
    VulkanShader create_shader(const ShaderDesc& desc);

    void update_descriptor_set(VulkanDescriptorSet& set,
                               const std::vector<DescriptorWrite>& writes);

    // Submit a recorded command buffer with synchronization info
    void submit(const VulkanCommandBuffer& cmd, const SubmitInfo& info = {});
    void wait_idle();

    // One-shot command submission for transfers, layout transitions, etc.
    void immediate_submit(std::function<void(VkCommandBuffer)>&& fn);

    // Native handle escape hatch
    template<typename T> T native_handle() const;

    // Vulkan-specific accessors (used by other Vulkan backend types)
    VkDevice device() const { return m_device; }
    VkPhysicalDevice physical_device() const { return m_physical_device; }
    VkQueue graphics_queue() const { return m_graphics_queue; }
    uint32_t graphics_queue_family() const { return m_graphics_queue_family; }
    VmaAllocator allocator() const { return m_allocator; }
    VkDescriptorPool descriptor_pool() const { return m_descriptor_pool; }
    VulkanContext& context() const { return *m_context; }

    explicit operator bool() const { return m_device != VK_NULL_HANDLE; }

private:
    void destroy();

    VulkanContext* m_context                  = nullptr;  // non-owning
    VkPhysicalDevice m_physical_device       = VK_NULL_HANDLE;
    VkDevice m_device                        = VK_NULL_HANDLE;
    VkQueue m_graphics_queue                 = VK_NULL_HANDLE;
    uint32_t m_graphics_queue_family         = 0;
    VmaAllocator m_allocator                 = VK_NULL_HANDLE;
    VkDescriptorPool m_descriptor_pool       = VK_NULL_HANDLE;

    // Immediate submit resources
    VkCommandPool m_immediate_cmd_pool       = VK_NULL_HANDLE;
    VkCommandBuffer m_immediate_cmd_buffer   = VK_NULL_HANDLE;
    VkFence m_immediate_fence                = VK_NULL_HANDLE;
};

// Template specializations for native_handle
template<> inline VkDevice VulkanDevice::native_handle<VkDevice>() const {
    return m_device;
}
template<> inline VkPhysicalDevice VulkanDevice::native_handle<VkPhysicalDevice>() const {
    return m_physical_device;
}
template<> inline VmaAllocator VulkanDevice::native_handle<VmaAllocator>() const {
    return m_allocator;
}

} // namespace helios::rhi::vulkan
```

- [ ] **Step 2: Create `helios-renderer/src/vulkan/vulkan_device.cpp`**

Adapt from `Engine/src/Renderer/Vulkan/VulkanDevice.cpp`. Key changes:

1. Constructor takes `VulkanContext& context` instead of using static `VulkanContext::GetInstance()`. Store `m_context = &context`.
2. Use `m_context->vkb_instance()` for physical device selection and VMA allocator creation.
3. Use `m_context->set_debug_name(...)` instead of `VulkanContext::SetDebugName(...)`.
4. Factory methods return value types, e.g. `return VulkanTexture(*this, desc, data);`
5. `submit()` takes `SubmitInfo` with optional semaphores/fence instead of bare `VkQueueSubmit`.
6. Implement `destroy()` private method; destructor and move-assignment both call it.
7. Move constructor/assignment: swap all members, leave source in null state.
8. Replace `HVE_CORE_*_TAG` with `spdlog::*`.

Implementation body follows the same structure as `VulkanDevice.cpp` lines 17-354, with the above substitutions. The full constructor should:
- Select physical device via vk-bootstrap (Vulkan 1.3, dynamic rendering, synchronization2)
- Create logical device
- Retrieve graphics queue + family index
- Create VMA allocator
- Create immediate submit command pool, buffer, fence
- Create descriptor pool
- Set debug names on all created objects

- [ ] **Step 3: Verify compilation**

---

### Task 7: `VulkanShader` -- SPIR-V loading, RAII

**Files:**
- Create: `helios-renderer/src/vulkan/vulkan_shader.h`
- Create: `helios-renderer/src/vulkan/vulkan_shader.cpp`

Adapt from: `Engine/src/Renderer/Vulkan/VulkanShader.h/.cpp`

- [ ] **Step 1: Create `helios-renderer/src/vulkan/vulkan_shader.h`**

```cpp
#pragma once

#include "rhi/rhi_types.h"
#include <vulkan/vulkan.h>
#include <string>

namespace helios::rhi::vulkan {

class VulkanDevice;

// RAII wrapper around VkShaderModule.
// Loads SPIR-V bytecode and creates the module in the constructor.
// Destructor destroys the module. Move-only.
class VulkanShader {
public:
    VulkanShader() = default;  // null/empty state
    VulkanShader(VulkanDevice& device, const ShaderDesc& desc);
    ~VulkanShader();

    VulkanShader(VulkanShader&& other) noexcept;
    VulkanShader& operator=(VulkanShader&& other) noexcept;
    VulkanShader(const VulkanShader&) = delete;
    VulkanShader& operator=(const VulkanShader&) = delete;

    VkShaderModule module() const { return m_module; }
    VkShaderStageFlagBits vk_stage() const;
    const char* entry_point() const { return m_entry_point.c_str(); }
    ShaderStage stage() const { return m_stage; }

    explicit operator bool() const { return m_module != VK_NULL_HANDLE; }

private:
    void destroy();

    VkShaderModule m_module    = VK_NULL_HANDLE;
    ShaderStage m_stage        = ShaderStage::Vertex;
    std::string m_entry_point  = "main";
    VulkanDevice* m_device     = nullptr;
};

} // namespace helios::rhi::vulkan
```

- [ ] **Step 2: Create `helios-renderer/src/vulkan/vulkan_shader.cpp`**

Adapt from `Engine/src/Renderer/Vulkan/VulkanShader.cpp`. Same logic:
- Constructor: `vkCreateShaderModule` with SPIR-V code from desc. Set debug name.
- Destructor: `vkDestroyShaderModule`.
- Move ctor/assignment: swap members, leave source null.
- `vk_stage()`: switch on `m_stage` to return `VkShaderStageFlagBits`.

---

### Task 8: `VulkanBuffer` -- RAII VkBuffer + VmaAllocation

**Files:**
- Create: `helios-renderer/src/vulkan/vulkan_buffer.h`
- Create: `helios-renderer/src/vulkan/vulkan_buffer.cpp`

Adapt from: `Engine/src/Renderer/Vulkan/VulkanBuffer.h/.cpp`

- [ ] **Step 1: Create `helios-renderer/src/vulkan/vulkan_buffer.h`**

```cpp
#pragma once

#include "rhi/rhi_types.h"
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

namespace helios::rhi::vulkan {

class VulkanDevice;

// RAII Vulkan buffer. Constructor allocates VkBuffer + VmaAllocation.
// Destructor frees both. Move-only.
class VulkanBuffer {
public:
    VulkanBuffer() = default;  // null/empty state
    VulkanBuffer(VulkanDevice& device, const BufferDesc& desc, const void* initial_data = nullptr);
    ~VulkanBuffer();

    VulkanBuffer(VulkanBuffer&& other) noexcept;
    VulkanBuffer& operator=(VulkanBuffer&& other) noexcept;
    VulkanBuffer(const VulkanBuffer&) = delete;
    VulkanBuffer& operator=(const VulkanBuffer&) = delete;

    // Update buffer contents. For CPU-visible buffers: direct memcpy.
    // For GPU-only buffers: staging buffer + ImmediateSubmit.
    void set_data(const void* data, uint32_t size, uint32_t offset = 0);

    void* map();
    void unmap();
    uint32_t size() const { return m_desc.size; }
    const BufferDesc& desc() const { return m_desc; }

    VkBuffer vk_buffer() const { return m_buffer; }

    template<typename T> T native_handle() const;

    explicit operator bool() const { return m_buffer != VK_NULL_HANDLE; }

private:
    void destroy();

    VkBuffer m_buffer              = VK_NULL_HANDLE;
    VmaAllocation m_allocation     = VK_NULL_HANDLE;
    VmaAllocationInfo m_alloc_info{};
    BufferDesc m_desc;
    VulkanDevice* m_device         = nullptr;
};

template<> inline VkBuffer VulkanBuffer::native_handle<VkBuffer>() const {
    return m_buffer;
}

} // namespace helios::rhi::vulkan
```

- [ ] **Step 2: Create `helios-renderer/src/vulkan/vulkan_buffer.cpp`**

Adapt from `Engine/src/Renderer/Vulkan/VulkanBuffer.cpp`. Key implementation:
- Constructor: `vmaCreateBuffer` with mapped buffer usage flags. Upload initial data via staging buffer (GPU_Only) or direct map (CPU-visible). Set debug name.
- `set_data()`: same staging/direct logic as existing code.
- `map()`/`unmap()`: `vmaMapMemory`/`vmaUnmapMemory`.
- Destructor: `vmaDestroyBuffer`.
- Move ctor/assignment: swap all members (`m_buffer`, `m_allocation`, `m_alloc_info`, `m_desc`, `m_device`), leave source null.

---

### Task 9: `VulkanTexture` -- RAII VkImage + VmaAllocation + VkImageView + VkSampler

**Files:**
- Create: `helios-renderer/src/vulkan/vulkan_texture.h`
- Create: `helios-renderer/src/vulkan/vulkan_texture.cpp`

Adapt from: `Engine/src/Renderer/Vulkan/VulkanTexture.h/.cpp`

- [ ] **Step 1: Create `helios-renderer/src/vulkan/vulkan_texture.h`**

```cpp
#pragma once

#include "rhi/rhi_types.h"
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

namespace helios::rhi::vulkan {

class VulkanDevice;

// RAII Vulkan texture. Constructor allocates VkImage + VmaAllocation +
// VkImageView + VkSampler. Destructor frees all. Move-only.
//
// Two construction modes:
//   1. Owned: allocates image via VMA (normal textures)
//   2. Wrapped: wraps an existing VkImage without ownership (swapchain images)
class VulkanTexture {
public:
    VulkanTexture() = default;  // null/empty state

    // Owned construction: allocates image via VMA
    VulkanTexture(VulkanDevice& device, const TextureDesc& desc,
                  const void* initial_data = nullptr);

    // Wrapped construction: wraps existing VkImage (e.g. swapchain images)
    // Does NOT own the image -- destructor will not free it.
    VulkanTexture(VulkanDevice& device, VkImage image, VkFormat format,
                  uint32_t width, uint32_t height, const char* debug_name);

    ~VulkanTexture();

    VulkanTexture(VulkanTexture&& other) noexcept;
    VulkanTexture& operator=(VulkanTexture&& other) noexcept;
    VulkanTexture(const VulkanTexture&) = delete;
    VulkanTexture& operator=(const VulkanTexture&) = delete;

    uint32_t width() const { return m_desc.width; }
    uint32_t height() const { return m_desc.height; }
    TextureFormat format() const { return m_desc.format; }
    const TextureDesc& desc() const { return m_desc; }

    VkImage vk_image() const { return m_image; }
    VkImageView vk_image_view() const { return m_image_view; }
    VkSampler vk_sampler() const { return m_sampler; }
    VkImageLayout current_layout() const { return m_current_layout; }
    void set_current_layout(VkImageLayout layout) { m_current_layout = layout; }

    template<typename T> T native_handle() const;

    explicit operator bool() const { return m_image != VK_NULL_HANDLE; }

    // Static utility: transition image layout via synchronization2
    static void transition_layout(VkCommandBuffer cmd, VkImage image,
                                  VkImageLayout old_layout, VkImageLayout new_layout,
                                  VkImageAspectFlags aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT);

private:
    void destroy();

    VkImage m_image                = VK_NULL_HANDLE;
    VmaAllocation m_allocation     = VK_NULL_HANDLE;
    VkImageView m_image_view       = VK_NULL_HANDLE;
    VkSampler m_sampler            = VK_NULL_HANDLE;
    VkImageLayout m_current_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    TextureDesc m_desc;
    VulkanDevice* m_device         = nullptr;
    bool m_owns_image              = true;
};

template<> inline VkImage VulkanTexture::native_handle<VkImage>() const {
    return m_image;
}
template<> inline VkImageView VulkanTexture::native_handle<VkImageView>() const {
    return m_image_view;
}

} // namespace helios::rhi::vulkan
```

- [ ] **Step 2: Create `helios-renderer/src/vulkan/vulkan_texture.cpp`**

Adapt from `Engine/src/Renderer/Vulkan/VulkanTexture.cpp` (all 343 lines). Key implementation:

1. **Owned constructor:** Create VkImage via VMA, create VkImageView, create VkSampler (using `to_vk_sampler_mode(desc.sampler)`), upload initial data via staging buffer + `device.immediate_submit()`, set debug names.
2. **Wrapped constructor:** Store existing VkImage, create VkImageView only, no sampler, `m_owns_image = false`.
3. **Destructor:** Destroy sampler, image view. If `m_owns_image`, destroy image via VMA.
4. **Move ctor/assignment:** Swap all members including `m_owns_image`. Source left null.
5. **`transition_layout()`:** Vulkan 1.3 synchronization2 barrier (same as existing code lines 311-341).

---

### Task 10: `VulkanRenderPass` + `VulkanFramebuffer` -- RAII

**Files:**
- Create: `helios-renderer/src/vulkan/vulkan_render_pass.h`
- Create: `helios-renderer/src/vulkan/vulkan_render_pass.cpp`
- Create: `helios-renderer/src/vulkan/vulkan_framebuffer.h`
- Create: `helios-renderer/src/vulkan/vulkan_framebuffer.cpp`

Adapt from: `Engine/src/Renderer/Vulkan/VulkanRenderPass.h/.cpp` and `VulkanFramebuffer.h/.cpp`

Note: The primary rendering path uses dynamic rendering (`vkCmdBeginRendering`), so `VkRenderPass` objects are primarily for ImGui compatibility and pipeline creation. They are still needed but are a secondary path.

- [ ] **Step 1: Create `helios-renderer/src/vulkan/vulkan_render_pass.h`**

```cpp
#pragma once

#include "rhi/rhi_types.h"
#include <vulkan/vulkan.h>

namespace helios::rhi::vulkan {

class VulkanDevice;

// RAII wrapper around VkRenderPass. Used for pipeline creation compatibility
// and ImGui. Primary rendering uses dynamic rendering (vkCmdBeginRendering).
class VulkanRenderPass {
public:
    VulkanRenderPass() = default;
    VulkanRenderPass(VulkanDevice& device, const RenderPassDesc& desc);
    ~VulkanRenderPass();

    VulkanRenderPass(VulkanRenderPass&& other) noexcept;
    VulkanRenderPass& operator=(VulkanRenderPass&& other) noexcept;
    VulkanRenderPass(const VulkanRenderPass&) = delete;
    VulkanRenderPass& operator=(const VulkanRenderPass&) = delete;

    VkRenderPass vk_render_pass() const { return m_render_pass; }
    const RenderPassDesc& desc() const { return m_desc; }

    explicit operator bool() const { return m_render_pass != VK_NULL_HANDLE; }

private:
    void destroy();

    VkRenderPass m_render_pass = VK_NULL_HANDLE;
    VulkanDevice* m_device     = nullptr;
    RenderPassDesc m_desc;
};

} // namespace helios::rhi::vulkan
```

- [ ] **Step 2: Create `helios-renderer/src/vulkan/vulkan_render_pass.cpp`**

Adapt from `Engine/src/Renderer/Vulkan/VulkanRenderPass.cpp`. Same attachment/subpass/dependency creation logic. Add move ctor/assignment.

- [ ] **Step 3: Create `helios-renderer/src/vulkan/vulkan_framebuffer.h`**

```cpp
#pragma once

#include "rhi/rhi_types.h"
#include <vulkan/vulkan.h>
#include <vector>

namespace helios::rhi::vulkan {

class VulkanDevice;
class VulkanRenderPass;
class VulkanTexture;

// RAII wrapper. Stores attachment views for dynamic rendering.
// Lazily creates VkFramebuffer for legacy render pass compatibility (ImGui).
class VulkanFramebuffer {
public:
    VulkanFramebuffer() = default;
    VulkanFramebuffer(VulkanDevice& device, const VulkanRenderPass* render_pass,
                      const std::vector<const VulkanTexture*>& attachments,
                      uint32_t width, uint32_t height,
                      const char* debug_name = nullptr);
    ~VulkanFramebuffer();

    VulkanFramebuffer(VulkanFramebuffer&& other) noexcept;
    VulkanFramebuffer& operator=(VulkanFramebuffer&& other) noexcept;
    VulkanFramebuffer(const VulkanFramebuffer&) = delete;
    VulkanFramebuffer& operator=(const VulkanFramebuffer&) = delete;

    uint32_t width() const { return m_width; }
    uint32_t height() const { return m_height; }
    const std::vector<VkImageView>& attachment_views() const { return m_attachment_views; }

    // Lazy VkFramebuffer creation for legacy render pass path
    VkFramebuffer get_or_create_vk_framebuffer();

    explicit operator bool() const { return !m_attachment_views.empty(); }

private:
    void destroy();

    VkFramebuffer m_framebuffer                = VK_NULL_HANDLE;
    VulkanDevice* m_device                     = nullptr;
    const VulkanRenderPass* m_render_pass      = nullptr;
    std::vector<VkImageView> m_attachment_views;
    uint32_t m_width                           = 0;
    uint32_t m_height                          = 0;
};

} // namespace helios::rhi::vulkan
```

- [ ] **Step 4: Create `helios-renderer/src/vulkan/vulkan_framebuffer.cpp`**

Adapt from `Engine/src/Renderer/Vulkan/VulkanFramebuffer.cpp`. Constructor extracts `VkImageView` from each `VulkanTexture*`. `get_or_create_vk_framebuffer()` lazily creates the `VkFramebuffer` object.

---

### Task 11: `VulkanDescriptorSetLayout` + `VulkanDescriptorSet` -- RAII

**Files:**
- Create: `helios-renderer/src/vulkan/vulkan_descriptor.h`
- Create: `helios-renderer/src/vulkan/vulkan_descriptor.cpp`

Adapt from: `Engine/src/Renderer/Vulkan/VulkanDescriptor.h/.cpp`

- [ ] **Step 1: Create `helios-renderer/src/vulkan/vulkan_descriptor.h`**

```cpp
#pragma once

#include "rhi/rhi_types.h"
#include <vulkan/vulkan.h>

namespace helios::rhi::vulkan {

class VulkanDevice;

// RAII wrapper around VkDescriptorSetLayout.
class VulkanDescriptorSetLayout {
public:
    VulkanDescriptorSetLayout() = default;
    VulkanDescriptorSetLayout(VulkanDevice& device, const DescriptorSetLayoutDesc& desc);
    ~VulkanDescriptorSetLayout();

    VulkanDescriptorSetLayout(VulkanDescriptorSetLayout&& other) noexcept;
    VulkanDescriptorSetLayout& operator=(VulkanDescriptorSetLayout&& other) noexcept;
    VulkanDescriptorSetLayout(const VulkanDescriptorSetLayout&) = delete;
    VulkanDescriptorSetLayout& operator=(const VulkanDescriptorSetLayout&) = delete;

    VkDescriptorSetLayout vk_layout() const { return m_layout; }
    explicit operator bool() const { return m_layout != VK_NULL_HANDLE; }

private:
    void destroy();

    VkDescriptorSetLayout m_layout = VK_NULL_HANDLE;
    VulkanDevice* m_device         = nullptr;
};

// RAII wrapper around VkDescriptorSet.
// Allocated from the device's descriptor pool.
// Freed when the pool is reset (or explicitly via vkFreeDescriptorSets
// since the pool uses VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT).
class VulkanDescriptorSet {
public:
    VulkanDescriptorSet() = default;
    VulkanDescriptorSet(VulkanDevice& device, VkDescriptorSet set);
    ~VulkanDescriptorSet();

    VulkanDescriptorSet(VulkanDescriptorSet&& other) noexcept;
    VulkanDescriptorSet& operator=(VulkanDescriptorSet&& other) noexcept;
    VulkanDescriptorSet(const VulkanDescriptorSet&) = delete;
    VulkanDescriptorSet& operator=(const VulkanDescriptorSet&) = delete;

    VkDescriptorSet vk_set() const { return m_set; }
    explicit operator bool() const { return m_set != VK_NULL_HANDLE; }

private:
    void destroy();

    VkDescriptorSet m_set  = VK_NULL_HANDLE;
    VulkanDevice* m_device = nullptr;
};

} // namespace helios::rhi::vulkan
```

- [ ] **Step 2: Create `helios-renderer/src/vulkan/vulkan_descriptor.cpp`**

Adapt from `Engine/src/Renderer/Vulkan/VulkanDescriptor.cpp`.
- `VulkanDescriptorSetLayout` constructor: build `VkDescriptorSetLayoutBinding` array from desc, call `vkCreateDescriptorSetLayout`. Set debug name.
- `VulkanDescriptorSetLayout` destructor: `vkDestroyDescriptorSetLayout`.
- `VulkanDescriptorSet` constructor: receives pre-allocated `VkDescriptorSet` (allocation happens in `VulkanDevice::create_descriptor_set`).
- `VulkanDescriptorSet` destructor: `vkFreeDescriptorSets` from the device pool.
- Both: move ctor/assignment with swap-and-null pattern.

---

### Task 12: `VulkanPipeline` -- graphics + compute, RAII

**Files:**
- Create: `helios-renderer/src/vulkan/vulkan_pipeline.h`
- Create: `helios-renderer/src/vulkan/vulkan_pipeline.cpp`

Adapt from: `Engine/src/Renderer/Vulkan/VulkanPipeline.h/.cpp`

- [ ] **Step 1: Create `helios-renderer/src/vulkan/vulkan_pipeline.h`**

```cpp
#pragma once

#include "rhi/rhi_types.h"
#include <vulkan/vulkan.h>

namespace helios::rhi::vulkan {

class VulkanDevice;

// RAII Vulkan pipeline. Supports both graphics and compute.
// Stores VkPipeline + VkPipelineLayout. Destructor destroys both.
class VulkanPipeline {
public:
    VulkanPipeline() = default;

    // Graphics pipeline
    VulkanPipeline(VulkanDevice& device, const GraphicsPipelineDesc& desc);
    // Compute pipeline
    VulkanPipeline(VulkanDevice& device, const ComputePipelineDesc& desc);

    ~VulkanPipeline();

    VulkanPipeline(VulkanPipeline&& other) noexcept;
    VulkanPipeline& operator=(VulkanPipeline&& other) noexcept;
    VulkanPipeline(const VulkanPipeline&) = delete;
    VulkanPipeline& operator=(const VulkanPipeline&) = delete;

    VkPipeline vk_pipeline() const { return m_pipeline; }
    VkPipelineLayout vk_layout() const { return m_pipeline_layout; }
    VkPipelineBindPoint bind_point() const { return m_bind_point; }

    explicit operator bool() const { return m_pipeline != VK_NULL_HANDLE; }

private:
    void destroy();

    VkPipeline m_pipeline              = VK_NULL_HANDLE;
    VkPipelineLayout m_pipeline_layout = VK_NULL_HANDLE;
    VkPipelineBindPoint m_bind_point   = VK_PIPELINE_BIND_POINT_GRAPHICS;
    VulkanDevice* m_device             = nullptr;
};

} // namespace helios::rhi::vulkan
```

- [ ] **Step 2: Create `helios-renderer/src/vulkan/vulkan_pipeline.cpp`**

Adapt from `Engine/src/Renderer/Vulkan/VulkanPipeline.cpp` (all 344 lines). Key changes:

1. `GraphicsPipelineDesc` uses `const void*` for shader/render_pass/layout pointers. Cast to concrete Vulkan types inside the constructor:
   ```cpp
   auto* vert = static_cast<const VulkanShader*>(desc.vertex_shader);
   auto* frag = static_cast<const VulkanShader*>(desc.fragment_shader);
   auto* rp   = static_cast<const VulkanRenderPass*>(desc.render_pass);
   ```
2. Same 11-step pipeline creation (shader stages, vertex input, input assembly, viewport, rasterization, multisampling, depth-stencil, color blend, pipeline layout, create pipeline, debug names).
3. Compute pipeline: same pattern.
4. Move ctor/assignment, `destroy()` helper.

---

### Task 13: `VulkanCommandBuffer` -- RAII, recording commands

**Files:**
- Create: `helios-renderer/src/vulkan/vulkan_command_buffer.h`
- Create: `helios-renderer/src/vulkan/vulkan_command_buffer.cpp`

Adapt from: `Engine/src/Renderer/Vulkan/VulkanCommandBuffer.h/.cpp`

- [ ] **Step 1: Create `helios-renderer/src/vulkan/vulkan_command_buffer.h`**

```cpp
#pragma once

#include "rhi/rhi_types.h"
#include <vulkan/vulkan.h>

namespace helios::rhi::vulkan {

class VulkanDevice;
class VulkanPipeline;
class VulkanBuffer;
class VulkanDescriptorSet;
class VulkanRenderPass;
class VulkanFramebuffer;

// RAII command buffer with its own command pool.
// Destructor destroys the pool (which implicitly frees the buffer).
class VulkanCommandBuffer {
public:
    VulkanCommandBuffer() = default;
    VulkanCommandBuffer(VulkanDevice& device);
    ~VulkanCommandBuffer();

    VulkanCommandBuffer(VulkanCommandBuffer&& other) noexcept;
    VulkanCommandBuffer& operator=(VulkanCommandBuffer&& other) noexcept;
    VulkanCommandBuffer(const VulkanCommandBuffer&) = delete;
    VulkanCommandBuffer& operator=(const VulkanCommandBuffer&) = delete;

    // Recording
    void begin();
    void end();

    // Render pass (Vulkan 1.3 dynamic rendering)
    void begin_render_pass(const VulkanRenderPass& render_pass,
                           const VulkanFramebuffer& framebuffer,
                           const ClearValues& clear);
    void end_render_pass();

    // Pipeline & state
    void bind_pipeline(const VulkanPipeline& pipeline);
    void set_viewport(float x, float y, float width, float height);
    void set_scissor(int32_t x, int32_t y, uint32_t width, uint32_t height);

    // Resources
    void bind_vertex_buffer(const VulkanBuffer& buffer, uint32_t binding = 0);
    void bind_index_buffer(const VulkanBuffer& buffer, IndexType type = IndexType::Uint32);
    void bind_descriptor_set(uint32_t set, const VulkanDescriptorSet& descriptor_set);
    void push_constants(ShaderStage stage, uint32_t offset, uint32_t size, const void* data);

    // Draw
    void draw(uint32_t vertex_count, uint32_t instance_count = 1,
              uint32_t first_vertex = 0);
    void draw_indexed(uint32_t index_count, uint32_t instance_count = 1,
                      uint32_t first_index = 0);

    // Compute
    void dispatch(uint32_t x, uint32_t y, uint32_t z);

    // Synchronization (Vulkan 1.3 synchronization2)
    void pipeline_barrier(const BarrierDesc& barrier);

    // Transfer
    void copy_buffer(const VulkanBuffer& src, const VulkanBuffer& dst, uint32_t size);

    VkCommandBuffer vk_command_buffer() const { return m_command_buffer; }

    explicit operator bool() const { return m_command_buffer != VK_NULL_HANDLE; }

private:
    void destroy();

    VkCommandBuffer m_command_buffer            = VK_NULL_HANDLE;
    VkCommandPool m_command_pool                = VK_NULL_HANDLE;
    VulkanDevice* m_device                      = nullptr;

    // Tracked state for push constants and descriptor binding
    VkPipelineLayout m_current_pipeline_layout  = VK_NULL_HANDLE;
    VkPipelineBindPoint m_current_bind_point    = VK_PIPELINE_BIND_POINT_GRAPHICS;
};

} // namespace helios::rhi::vulkan
```

- [ ] **Step 2: Create `helios-renderer/src/vulkan/vulkan_command_buffer.cpp`**

Adapt from `Engine/src/Renderer/Vulkan/VulkanCommandBuffer.cpp` (all 273 lines). Key changes:
1. Methods take `const VulkanPipeline&`, `const VulkanBuffer&`, etc. instead of `RHIPipeline*`, `RHIBuffer*`. No `static_cast` needed -- types are concrete.
2. `begin_render_pass()` uses dynamic rendering (`vkCmdBeginRendering`) as in existing code.
3. `set_viewport()` uses negative-height trick for Y-flip (same as existing).
4. Move ctor/assignment, `destroy()`.

---

### Task 14: `VulkanSwapchain` -- RAII, reconstruct-on-resize

**Files:**
- Create: `helios-renderer/src/vulkan/vulkan_swapchain.h`
- Create: `helios-renderer/src/vulkan/vulkan_swapchain.cpp`

Adapt from: `Engine/src/Renderer/Vulkan/VulkanSwapchain.h/.cpp`

Key changes: No `Resize()` method. To resize, destroy old swapchain (via move-assignment or destruction) and construct a new one. The `SwapchainDesc` includes the `VkSurfaceKHR` and desired dimensions.

- [ ] **Step 1: Create `helios-renderer/src/vulkan/vulkan_swapchain.h`**

```cpp
#pragma once

#include "rhi/rhi_types.h"
#include <vulkan/vulkan.h>
#include <vector>

namespace helios::rhi::vulkan {

class VulkanDevice;

// RAII swapchain. Constructor creates VkSwapchainKHR + sync primitives.
// Destructor destroys everything.
//
// No Resize() method. To resize:
//   device.wait_idle();
//   swapchain = device.create_swapchain(SwapchainDesc{.width=new_w, .height=new_h, ...});
// The move-assignment destroys the old swapchain first.
class VulkanSwapchain {
public:
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

    VulkanSwapchain() = default;
    VulkanSwapchain(VulkanDevice& device, const SwapchainDesc& desc);
    ~VulkanSwapchain();

    VulkanSwapchain(VulkanSwapchain&& other) noexcept;
    VulkanSwapchain& operator=(VulkanSwapchain&& other) noexcept;
    VulkanSwapchain(const VulkanSwapchain&) = delete;
    VulkanSwapchain& operator=(const VulkanSwapchain&) = delete;

    // Frame operations
    bool acquire_next_image();
    void present();

    // Accessors
    uint32_t image_count() const { return static_cast<uint32_t>(m_images.size()); }
    uint32_t current_image_index() const { return m_current_image_index; }
    uint32_t current_frame() const { return m_current_frame; }
    uint32_t width() const { return m_extent.width; }
    uint32_t height() const { return m_extent.height; }
    VkFormat vk_format() const { return m_image_format; }

    VkSwapchainKHR vk_swapchain() const { return m_swapchain; }
    VkImage current_vk_image() const { return m_images[m_current_image_index]; }
    VkImageView current_image_view() const { return m_image_views[m_current_image_index]; }
    const std::vector<VkImage>& images() const { return m_images; }
    const std::vector<VkImageView>& image_views() const { return m_image_views; }

    // Sync objects for the current frame-in-flight
    VkSemaphore image_available_semaphore() const {
        return m_image_available[m_current_frame];
    }
    VkSemaphore render_finished_semaphore() const {
        return m_render_finished[m_current_frame];
    }
    VkFence in_flight_fence() const {
        return m_in_flight_fences[m_current_frame];
    }

    explicit operator bool() const { return m_swapchain != VK_NULL_HANDLE; }

private:
    void create_swapchain(const SwapchainDesc& desc);
    void create_sync_objects();
    void destroy();

    VulkanDevice* m_device                = nullptr;
    VkSurfaceKHR m_surface                = VK_NULL_HANDLE;
    VkSwapchainKHR m_swapchain            = VK_NULL_HANDLE;

    std::vector<VkImage> m_images;
    std::vector<VkImageView> m_image_views;
    VkFormat m_image_format               = VK_FORMAT_UNDEFINED;
    VkExtent2D m_extent{};

    uint32_t m_current_image_index        = 0;
    uint32_t m_current_frame              = 0;

    VkSemaphore m_image_available[MAX_FRAMES_IN_FLIGHT]{};
    VkSemaphore m_render_finished[MAX_FRAMES_IN_FLIGHT]{};
    VkFence m_in_flight_fences[MAX_FRAMES_IN_FLIGHT]{};
};

} // namespace helios::rhi::vulkan
```

- [ ] **Step 2: Create `helios-renderer/src/vulkan/vulkan_swapchain.cpp`**

Adapt from `Engine/src/Renderer/Vulkan/VulkanSwapchain.cpp` (all 248 lines). Key changes:
1. Constructor calls `create_swapchain(desc)` + `create_sync_objects()`.
2. `create_swapchain()` takes `SwapchainDesc` instead of raw width/height. Uses `to_vk_present_mode(desc.present_mode)` and `static_cast<VkSurfaceKHR>(desc.surface)`.
3. No `Resize()` method.
4. `destroy()`: destroys sync objects, image views, swapchain. Called by destructor and move-assignment.
5. Move ctor/assignment: swap all members.
6. `acquire_next_image()`: same logic (wait fence, acquire, reset fence). Returns false on `VK_ERROR_OUT_OF_DATE_KHR`.
7. `present()`: same logic. Advances `m_current_frame`.

---

### Task 15: `PipelineCache` -- caches pipelines by name, loads shaders

**Files:**
- Create: `helios-renderer/src/pipeline_cache.h`
- Create: `helios-renderer/src/pipeline_cache.cpp`

- [ ] **Step 1: Create `helios-renderer/src/pipeline_cache.h`**

```cpp
#pragma once

#include "rhi/rhi_types.h"
#include <string>
#include <unordered_map>
#include <vector>
#include <optional>
#include <filesystem>

namespace helios::rhi::vulkan {
class VulkanDevice;
class VulkanPipeline;
class VulkanShader;
}

namespace helios::rhi {

// Caches compiled pipelines and loaded shaders by name.
// Avoids redundant GPU resource creation for the same pipeline/shader.
class PipelineCache {
public:
    explicit PipelineCache(vulkan::VulkanDevice& device);
    ~PipelineCache() = default;

    PipelineCache(PipelineCache&&) noexcept = default;
    PipelineCache& operator=(PipelineCache&&) noexcept = default;
    PipelineCache(const PipelineCache&) = delete;
    PipelineCache& operator=(const PipelineCache&) = delete;

    // Load a SPIR-V shader from disk. Caches by name.
    // Returns pointer to cached shader (non-owning, valid for cache lifetime).
    vulkan::VulkanShader* load_shader(const std::string& name,
                                       const std::filesystem::path& spirv_path,
                                       ShaderStage stage);

    // Get or create a graphics pipeline. Caches by name.
    vulkan::VulkanPipeline* get_or_create_graphics_pipeline(
        const std::string& name,
        const GraphicsPipelineDesc& desc);

    // Get or create a compute pipeline. Caches by name.
    vulkan::VulkanPipeline* get_or_create_compute_pipeline(
        const std::string& name,
        const ComputePipelineDesc& desc);

    // Retrieve cached resources (returns nullptr if not found)
    vulkan::VulkanShader* get_shader(const std::string& name);
    vulkan::VulkanPipeline* get_pipeline(const std::string& name);

    // Clear all cached resources (GPU objects destroyed via RAII)
    void clear();

private:
    vulkan::VulkanDevice* m_device;
    std::unordered_map<std::string, vulkan::VulkanShader> m_shaders;
    std::unordered_map<std::string, vulkan::VulkanPipeline> m_pipelines;
};

} // namespace helios::rhi
```

- [ ] **Step 2: Create `helios-renderer/src/pipeline_cache.cpp`**

```cpp
#include "pipeline_cache.h"
#include "vulkan/vulkan_device.h"
#include "vulkan/vulkan_pipeline.h"
#include "vulkan/vulkan_shader.h"

#include <fstream>
#include <spdlog/spdlog.h>

namespace helios::rhi {

PipelineCache::PipelineCache(vulkan::VulkanDevice& device)
    : m_device(&device)
{
}

vulkan::VulkanShader* PipelineCache::load_shader(
    const std::string& name,
    const std::filesystem::path& spirv_path,
    ShaderStage stage)
{
    // Return cached if exists
    auto it = m_shaders.find(name);
    if (it != m_shaders.end()) {
        return &it->second;
    }

    // Read SPIR-V file
    std::ifstream file(spirv_path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        spdlog::error("[PipelineCache] Failed to open SPIR-V file: {}",
                      spirv_path.string());
        return nullptr;
    }

    auto file_size = file.tellg();
    std::vector<uint8_t> spirv_code(static_cast<size_t>(file_size));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(spirv_code.data()), file_size);

    ShaderDesc desc;
    desc.stage       = stage;
    desc.spirv_code  = std::move(spirv_code);
    desc.entry_point = "main";
    desc.debug_name  = name;

    auto [inserted_it, success] = m_shaders.emplace(
        name, m_device->create_shader(desc));

    if (!success || !inserted_it->second) {
        spdlog::error("[PipelineCache] Failed to create shader '{}'", name);
        m_shaders.erase(name);
        return nullptr;
    }

    return &inserted_it->second;
}

vulkan::VulkanPipeline* PipelineCache::get_or_create_graphics_pipeline(
    const std::string& name,
    const GraphicsPipelineDesc& desc)
{
    auto it = m_pipelines.find(name);
    if (it != m_pipelines.end()) {
        return &it->second;
    }

    auto [inserted_it, success] = m_pipelines.emplace(
        name, m_device->create_graphics_pipeline(desc));

    if (!success || !inserted_it->second) {
        spdlog::error("[PipelineCache] Failed to create graphics pipeline '{}'", name);
        m_pipelines.erase(name);
        return nullptr;
    }

    return &inserted_it->second;
}

vulkan::VulkanPipeline* PipelineCache::get_or_create_compute_pipeline(
    const std::string& name,
    const ComputePipelineDesc& desc)
{
    auto it = m_pipelines.find(name);
    if (it != m_pipelines.end()) {
        return &it->second;
    }

    auto [inserted_it, success] = m_pipelines.emplace(
        name, m_device->create_compute_pipeline(desc));

    if (!success || !inserted_it->second) {
        spdlog::error("[PipelineCache] Failed to create compute pipeline '{}'", name);
        m_pipelines.erase(name);
        return nullptr;
    }

    return &inserted_it->second;
}

vulkan::VulkanShader* PipelineCache::get_shader(const std::string& name)
{
    auto it = m_shaders.find(name);
    return it != m_shaders.end() ? &it->second : nullptr;
}

vulkan::VulkanPipeline* PipelineCache::get_pipeline(const std::string& name)
{
    auto it = m_pipelines.find(name);
    return it != m_pipelines.end() ? &it->second : nullptr;
}

void PipelineCache::clear()
{
    m_pipelines.clear();
    m_shaders.clear();
}

} // namespace helios::rhi
```

---

### Task 16: VulkanDevice factory method implementations

**Files:**
- Modify: `helios-renderer/src/vulkan/vulkan_device.cpp` (add factory method bodies)

This task completes the factory methods declared in Task 6 that depend on all the types created in Tasks 7-14.

- [ ] **Step 1: Implement all factory methods in `vulkan_device.cpp`**

Add the following implementations after the constructor/destructor/move code. Each factory method constructs the RAII type by value and returns it (relying on move semantics / NRVO):

```cpp
VulkanTexture VulkanDevice::create_texture(const TextureDesc& desc, const void* data) {
    return VulkanTexture(*this, desc, data);
}

VulkanBuffer VulkanDevice::create_buffer(const BufferDesc& desc, const void* data) {
    return VulkanBuffer(*this, desc, data);
}

VulkanShader VulkanDevice::create_shader(const ShaderDesc& desc) {
    return VulkanShader(*this, desc);
}

VulkanPipeline VulkanDevice::create_graphics_pipeline(const GraphicsPipelineDesc& desc) {
    return VulkanPipeline(*this, desc);
}

VulkanPipeline VulkanDevice::create_compute_pipeline(const ComputePipelineDesc& desc) {
    return VulkanPipeline(*this, desc);
}

VulkanCommandBuffer VulkanDevice::create_command_buffer() {
    return VulkanCommandBuffer(*this);
}

VulkanSwapchain VulkanDevice::create_swapchain(const SwapchainDesc& desc) {
    return VulkanSwapchain(*this, desc);
}

VulkanDescriptorSetLayout VulkanDevice::create_descriptor_set_layout(
    const DescriptorSetLayoutDesc& desc) {
    return VulkanDescriptorSetLayout(*this, desc);
}

VulkanDescriptorSet VulkanDevice::create_descriptor_set(
    const VulkanDescriptorSetLayout& layout) {
    VkDescriptorSetLayout dsl = layout.vk_layout();

    VkDescriptorSetAllocateInfo alloc_info{};
    alloc_info.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool     = m_descriptor_pool;
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts        = &dsl;

    VkDescriptorSet set;
    VkResult result = vkAllocateDescriptorSets(m_device, &alloc_info, &set);
    if (result != VK_SUCCESS) {
        spdlog::error("[Vulkan] Failed to allocate descriptor set");
        return VulkanDescriptorSet{};  // return empty/null
    }

    return VulkanDescriptorSet(*this, set);
}

VulkanRenderPass VulkanDevice::create_render_pass(const RenderPassDesc& desc) {
    return VulkanRenderPass(*this, desc);
}

VulkanFramebuffer VulkanDevice::create_framebuffer(const FramebufferDesc& desc) {
    // FramebufferDesc in the new API is minimal; actual construction uses
    // the overload in VulkanFramebuffer that takes attachments directly.
    // This factory is a convenience for the simple case.
    return VulkanFramebuffer{};
}
```

- [ ] **Step 2: Implement `update_descriptor_set()`**

Adapt from `Engine/src/Renderer/Vulkan/VulkanDevice.cpp` lines 264-331. Cast `DescriptorWrite::buffer_handle` and `texture_handle` to `VulkanBuffer*`/`VulkanTexture*`.

- [ ] **Step 3: Implement `submit()`**

```cpp
void VulkanDevice::submit(const VulkanCommandBuffer& cmd, const SubmitInfo& info) {
    VkCommandBuffer vk_cmd = cmd.vk_command_buffer();

    VkSemaphore wait_sem = static_cast<VkSemaphore>(info.wait_semaphore);
    VkSemaphore signal_sem = static_cast<VkSemaphore>(info.signal_semaphore);
    VkFence fence = static_cast<VkFence>(info.fence);

    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo submit_info{};
    submit_info.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount   = 1;
    submit_info.pCommandBuffers      = &vk_cmd;

    if (wait_sem) {
        submit_info.waitSemaphoreCount = 1;
        submit_info.pWaitSemaphores    = &wait_sem;
        submit_info.pWaitDstStageMask  = &wait_stage;
    }

    if (signal_sem) {
        submit_info.signalSemaphoreCount = 1;
        submit_info.pSignalSemaphores    = &signal_sem;
    }

    vkQueueSubmit(m_graphics_queue, 1, &submit_info,
                  fence ? fence : VK_NULL_HANDLE);
}
```

- [ ] **Step 4: Verify all factory methods compile together**

---

### Task 17: Tests -- device creation, RAII verification, native handles

**Files:**
- Create: `helios-renderer/tests/CMakeLists.txt`
- Create: `helios-renderer/tests/test_rhi_types.cpp`
- Create: `helios-renderer/tests/test_vulkan_device.cpp`
- Create: `helios-renderer/tests/test_vulkan_resources.cpp`

- [ ] **Step 1: Create `helios-renderer/tests/CMakeLists.txt`**

```cmake
find_package(GTest REQUIRED)

add_executable(helios-renderer-tests
    test_rhi_types.cpp
    test_vulkan_device.cpp
    test_vulkan_resources.cpp
)

target_link_libraries(helios-renderer-tests
    PRIVATE
        helios-renderer
        GTest::gtest_main
)

target_include_directories(helios-renderer-tests PRIVATE
    ${CMAKE_SOURCE_DIR}/helios-renderer/src
)

include(GoogleTest)
gtest_discover_tests(helios-renderer-tests)
```

- [ ] **Step 2: Create `helios-renderer/tests/test_rhi_types.cpp`**

Tests for enum bitfield operators and descriptor struct defaults:

```cpp
#include <gtest/gtest.h>
#include "rhi/rhi_types.h"

using namespace helios::rhi;

TEST(RHITypes, BufferUsageBitfieldOr) {
    auto usage = BufferUsage::Vertex | BufferUsage::Index;
    EXPECT_TRUE(has_flag(usage, BufferUsage::Vertex));
    EXPECT_TRUE(has_flag(usage, BufferUsage::Index));
    EXPECT_FALSE(has_flag(usage, BufferUsage::Uniform));
}

TEST(RHITypes, TextureUsageBitfieldAnd) {
    auto usage = TextureUsage::Sampled | TextureUsage::ColorAttachment;
    auto masked = usage & TextureUsage::Sampled;
    EXPECT_TRUE(has_flag(masked, TextureUsage::Sampled));
}

TEST(RHITypes, ShaderStageBitfieldCombine) {
    auto stage = ShaderStage::Vertex | ShaderStage::Fragment;
    EXPECT_TRUE(has_flag(stage, ShaderStage::Vertex));
    EXPECT_TRUE(has_flag(stage, ShaderStage::Fragment));
    EXPECT_FALSE(has_flag(stage, ShaderStage::Compute));
}

TEST(RHITypes, ShaderStageBitfieldNot) {
    auto all = ShaderStage::Vertex | ShaderStage::Fragment | ShaderStage::Compute;
    auto without_compute = all & ~ShaderStage::Compute;
    EXPECT_TRUE(has_flag(without_compute, ShaderStage::Vertex));
    EXPECT_FALSE(has_flag(without_compute, ShaderStage::Compute));
}

TEST(RHITypes, TextureDescDefaults) {
    TextureDesc desc;
    EXPECT_EQ(desc.width, 1u);
    EXPECT_EQ(desc.height, 1u);
    EXPECT_EQ(desc.format, TextureFormat::RGBA8);
    EXPECT_EQ(desc.mip_levels, 1u);
    EXPECT_EQ(desc.sampler, SamplerMode::Repeat);
}

TEST(RHITypes, BufferDescDefaults) {
    BufferDesc desc;
    EXPECT_EQ(desc.size, 0u);
    EXPECT_EQ(desc.usage, BufferUsage::Vertex);
    EXPECT_EQ(desc.access, MemoryAccess::GPU_Only);
}
```

- [ ] **Step 3: Create `helios-renderer/tests/test_vulkan_device.cpp`**

Tests that require a live Vulkan instance. These tests will be skipped in CI without a GPU. Use a test fixture that creates VulkanContext + VulkanDevice in SetUp and destroys in TearDown.

```cpp
#include <gtest/gtest.h>
#include "vulkan/vulkan_context.h"
#include "vulkan/vulkan_device.h"

using namespace helios::rhi::vulkan;

class VulkanDeviceTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a headless Vulkan instance (no window surface needed for device creation tests)
        m_context = std::make_unique<VulkanContext>("HeliosTest", true);
        if (!*m_context) {
            GTEST_SKIP() << "Vulkan instance creation failed (no Vulkan support?)";
        }

        // For device creation without a swapchain, pass VK_NULL_HANDLE as surface.
        // Some tests may need a real surface from GLFW -- those are in a separate fixture.
        m_device = std::make_unique<VulkanDevice>(*m_context, VK_NULL_HANDLE);
        if (!*m_device) {
            GTEST_SKIP() << "Vulkan device creation failed";
        }
    }

    void TearDown() override {
        if (m_device) m_device->wait_idle();
        m_device.reset();
        m_context.reset();
    }

    std::unique_ptr<VulkanContext> m_context;
    std::unique_ptr<VulkanDevice> m_device;
};

TEST_F(VulkanDeviceTest, DeviceCreation) {
    EXPECT_NE(m_device->device(), VK_NULL_HANDLE);
    EXPECT_NE(m_device->physical_device(), VK_NULL_HANDLE);
    EXPECT_NE(m_device->allocator(), VK_NULL_HANDLE);
    EXPECT_NE(m_device->graphics_queue(), VK_NULL_HANDLE);
}

TEST_F(VulkanDeviceTest, NativeHandleDevice) {
    auto handle = m_device->native_handle<VkDevice>();
    EXPECT_EQ(handle, m_device->device());
    EXPECT_NE(handle, VK_NULL_HANDLE);
}

TEST_F(VulkanDeviceTest, NativeHandlePhysicalDevice) {
    auto handle = m_device->native_handle<VkPhysicalDevice>();
    EXPECT_EQ(handle, m_device->physical_device());
}

TEST_F(VulkanDeviceTest, NativeHandleAllocator) {
    auto handle = m_device->native_handle<VmaAllocator>();
    EXPECT_EQ(handle, m_device->allocator());
}

TEST_F(VulkanDeviceTest, WaitIdleDoesNotCrash) {
    EXPECT_NO_THROW(m_device->wait_idle());
}

TEST_F(VulkanDeviceTest, MoveConstruct) {
    VkDevice original_handle = m_device->device();
    VulkanDevice moved(std::move(*m_device));

    EXPECT_EQ(moved.device(), original_handle);
    EXPECT_EQ(m_device->device(), VK_NULL_HANDLE);  // source nulled

    // Clean up moved device properly
    moved.wait_idle();
    m_device = std::make_unique<VulkanDevice>(std::move(moved));
}
```

- [ ] **Step 4: Create `helios-renderer/tests/test_vulkan_resources.cpp`**

Tests for RAII resource creation and destruction:

```cpp
#include <gtest/gtest.h>
#include "vulkan/vulkan_context.h"
#include "vulkan/vulkan_device.h"
#include "vulkan/vulkan_buffer.h"
#include "vulkan/vulkan_texture.h"
#include "vulkan/vulkan_descriptor.h"
#include "vulkan/vulkan_command_buffer.h"
#include "rhi/rhi_types.h"

using namespace helios::rhi;
using namespace helios::rhi::vulkan;

class VulkanResourceTest : public ::testing::Test {
protected:
    void SetUp() override {
        m_context = std::make_unique<VulkanContext>("HeliosResourceTest", true);
        if (!*m_context) GTEST_SKIP() << "No Vulkan support";

        m_device = std::make_unique<VulkanDevice>(*m_context, VK_NULL_HANDLE);
        if (!*m_device) GTEST_SKIP() << "No Vulkan device";
    }

    void TearDown() override {
        if (m_device) m_device->wait_idle();
        m_device.reset();
        m_context.reset();
    }

    std::unique_ptr<VulkanContext> m_context;
    std::unique_ptr<VulkanDevice> m_device;
};

// --- Buffer tests ---

TEST_F(VulkanResourceTest, BufferCreateDestroy) {
    BufferDesc desc;
    desc.size       = 1024;
    desc.usage      = BufferUsage::Uniform;
    desc.access     = MemoryAccess::CPU_to_GPU;
    desc.debug_name = "TestBuffer";

    auto buffer = m_device->create_buffer(desc);
    EXPECT_TRUE(static_cast<bool>(buffer));
    EXPECT_NE(buffer.vk_buffer(), VK_NULL_HANDLE);
    EXPECT_EQ(buffer.size(), 1024u);
    // RAII: buffer destroyed when it goes out of scope
}

TEST_F(VulkanResourceTest, BufferSetData) {
    BufferDesc desc;
    desc.size       = 64;
    desc.usage      = BufferUsage::Uniform;
    desc.access     = MemoryAccess::CPU_to_GPU;
    desc.debug_name = "SetDataBuffer";

    auto buffer = m_device->create_buffer(desc);
    ASSERT_TRUE(static_cast<bool>(buffer));

    float data[16] = {1.0f, 2.0f, 3.0f};
    EXPECT_NO_THROW(buffer.set_data(data, sizeof(data)));
}

TEST_F(VulkanResourceTest, BufferMoveOnly) {
    BufferDesc desc;
    desc.size   = 256;
    desc.usage  = BufferUsage::Vertex;
    desc.access = MemoryAccess::CPU_to_GPU;

    auto buf1 = m_device->create_buffer(desc);
    VkBuffer original = buf1.vk_buffer();

    VulkanBuffer buf2 = std::move(buf1);
    EXPECT_EQ(buf2.vk_buffer(), original);
    EXPECT_EQ(buf1.vk_buffer(), VK_NULL_HANDLE);  // source nulled
}

// --- Texture tests ---

TEST_F(VulkanResourceTest, TextureCreateDestroy) {
    TextureDesc desc;
    desc.width      = 64;
    desc.height     = 64;
    desc.format     = TextureFormat::RGBA8;
    desc.usage      = TextureUsage::Sampled;
    desc.debug_name = "TestTexture";

    auto texture = m_device->create_texture(desc);
    EXPECT_TRUE(static_cast<bool>(texture));
    EXPECT_NE(texture.vk_image(), VK_NULL_HANDLE);
    EXPECT_NE(texture.vk_image_view(), VK_NULL_HANDLE);
    EXPECT_NE(texture.vk_sampler(), VK_NULL_HANDLE);
    EXPECT_EQ(texture.width(), 64u);
    EXPECT_EQ(texture.height(), 64u);
}

TEST_F(VulkanResourceTest, TextureWithInitialData) {
    TextureDesc desc;
    desc.width      = 4;
    desc.height     = 4;
    desc.format     = TextureFormat::RGBA8;
    desc.usage      = TextureUsage::Sampled;
    desc.debug_name = "DataTexture";

    std::vector<uint8_t> pixels(4 * 4 * 4, 255);  // 4x4 RGBA white
    auto texture = m_device->create_texture(desc, pixels.data());
    EXPECT_TRUE(static_cast<bool>(texture));
}

TEST_F(VulkanResourceTest, TextureNativeHandles) {
    TextureDesc desc;
    desc.width  = 16;
    desc.height = 16;
    desc.format = TextureFormat::RGBA8;
    desc.usage  = TextureUsage::Sampled;

    auto texture = m_device->create_texture(desc);
    ASSERT_TRUE(static_cast<bool>(texture));

    EXPECT_EQ(texture.native_handle<VkImage>(), texture.vk_image());
    EXPECT_EQ(texture.native_handle<VkImageView>(), texture.vk_image_view());
}

TEST_F(VulkanResourceTest, TextureMoveOnly) {
    TextureDesc desc;
    desc.width  = 8;
    desc.height = 8;
    desc.format = TextureFormat::RGBA8;
    desc.usage  = TextureUsage::Sampled;

    auto tex1 = m_device->create_texture(desc);
    VkImage original = tex1.vk_image();

    VulkanTexture tex2 = std::move(tex1);
    EXPECT_EQ(tex2.vk_image(), original);
    EXPECT_EQ(tex1.vk_image(), VK_NULL_HANDLE);
}

// --- Descriptor set layout tests ---

TEST_F(VulkanResourceTest, DescriptorSetLayoutCreateDestroy) {
    DescriptorSetLayoutDesc desc;
    desc.bindings = {
        {0, DescriptorType::UniformBuffer, ShaderStage::Vertex, 1},
        {1, DescriptorType::CombinedImageSampler, ShaderStage::Fragment, 1},
    };
    desc.debug_name = "TestLayout";

    auto layout = m_device->create_descriptor_set_layout(desc);
    EXPECT_TRUE(static_cast<bool>(layout));
    EXPECT_NE(layout.vk_layout(), VK_NULL_HANDLE);
}

// --- Command buffer tests ---

TEST_F(VulkanResourceTest, CommandBufferCreateDestroy) {
    auto cmd = m_device->create_command_buffer();
    EXPECT_TRUE(static_cast<bool>(cmd));
    EXPECT_NE(cmd.vk_command_buffer(), VK_NULL_HANDLE);
}

TEST_F(VulkanResourceTest, CommandBufferBeginEnd) {
    auto cmd = m_device->create_command_buffer();
    ASSERT_TRUE(static_cast<bool>(cmd));

    EXPECT_NO_THROW(cmd.begin());
    EXPECT_NO_THROW(cmd.end());
}

// --- Swapchain tests ---
// Note: Swapchain requires a VkSurfaceKHR from a real window (GLFW).
// These tests are skipped unless Plan 3 (Window) provides a test surface.
// A placeholder test verifies that creating with null surface fails gracefully.

TEST_F(VulkanResourceTest, SwapchainNullSurfaceHandledGracefully) {
    SwapchainDesc desc;
    desc.width        = 800;
    desc.height       = 600;
    desc.surface      = nullptr;  // no surface
    desc.present_mode = PresentMode::Fifo;

    // Should either fail gracefully or be skippable; should NOT crash.
    // The exact behavior depends on implementation -- may log an error and
    // return an empty swapchain.
    EXPECT_NO_FATAL_FAILURE({
        auto sc = m_device->create_swapchain(desc);
        // sc may be invalid, but should not segfault
    });
}
```

- [ ] **Step 5: Add tests to the parent CMakeLists.txt and verify they compile**

```bash
cmake --build build --target helios-renderer-tests
```

- [ ] **Step 6: Run the tests**

```bash
cd build && ctest --test-dir . -R helios-renderer-tests --output-on-failure
```

---

### Task 18: Final integration build and smoke test

**Files:**
- Modify: `helios-renderer/CMakeLists.txt` (add test subdirectory)
- Verify: all headers included by `rhi/rhi.h` compile together

- [ ] **Step 1: Add test subdirectory to CMake**

Add to `helios-renderer/CMakeLists.txt`:

```cmake
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
```

- [ ] **Step 2: Create a minimal integration .cpp that includes `rhi/rhi.h` and uses all type aliases**

Create `helios-renderer/tests/test_rhi_integration.cpp`:

```cpp
// Integration test: verify all type aliases resolve and basic operations compile.
#include "rhi/rhi.h"
#include <gtest/gtest.h>
#include <type_traits>

using namespace helios::rhi;

// Static assertions: all RHI types are move-constructible and not copy-constructible
static_assert(std::is_move_constructible_v<Device>);
static_assert(std::is_move_constructible_v<Texture>);
static_assert(std::is_move_constructible_v<Buffer>);
static_assert(std::is_move_constructible_v<Pipeline>);
static_assert(std::is_move_constructible_v<CommandBuffer>);
static_assert(std::is_move_constructible_v<Swapchain>);
static_assert(std::is_move_constructible_v<DescriptorSet>);
static_assert(std::is_move_constructible_v<DescriptorSetLayout>);
static_assert(std::is_move_constructible_v<RenderPass>);
static_assert(std::is_move_constructible_v<Framebuffer>);
static_assert(std::is_move_constructible_v<Shader>);

static_assert(!std::is_copy_constructible_v<Device>);
static_assert(!std::is_copy_constructible_v<Texture>);
static_assert(!std::is_copy_constructible_v<Buffer>);
static_assert(!std::is_copy_constructible_v<Pipeline>);
static_assert(!std::is_copy_constructible_v<CommandBuffer>);
static_assert(!std::is_copy_constructible_v<Swapchain>);
static_assert(!std::is_copy_constructible_v<DescriptorSet>);
static_assert(!std::is_copy_constructible_v<DescriptorSetLayout>);
static_assert(!std::is_copy_constructible_v<RenderPass>);
static_assert(!std::is_copy_constructible_v<Framebuffer>);
static_assert(!std::is_copy_constructible_v<Shader>);

TEST(RHIIntegration, TypeAliasesCompile) {
    // This test passes if the file compiles -- static_asserts above
    // verify the type properties at compile time.
    SUCCEED();
}
```

Add this file to the test target in `helios-renderer/tests/CMakeLists.txt`.

- [ ] **Step 3: Full build and test run**

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build -R helios-renderer-tests --output-on-failure
```

- [ ] **Step 4: Verify no compiler warnings with strict flags**

```bash
cmake -B build-warnings \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror" \
    -DBUILD_TESTING=ON
cmake --build build-warnings --target helios-renderer 2>&1 | head -50
```

Fix any warnings in helios-renderer source (not in vendor code -- those are suppressed via SYSTEM include paths or compiler flags, per project convention of never editing vendored source).

---

## Summary of files created

```
helios-renderer/
  CMakeLists.txt
  src/
    rhi/
      rhi_types.h
      rhi.h
    vulkan/
      vulkan_vma.cpp
      vulkan_context.h
      vulkan_context.cpp
      vulkan_utils.h
      vulkan_device.h
      vulkan_device.cpp
      vulkan_shader.h
      vulkan_shader.cpp
      vulkan_buffer.h
      vulkan_buffer.cpp
      vulkan_texture.h
      vulkan_texture.cpp
      vulkan_render_pass.h
      vulkan_render_pass.cpp
      vulkan_framebuffer.h
      vulkan_framebuffer.cpp
      vulkan_descriptor.h
      vulkan_descriptor.cpp
      vulkan_pipeline.h
      vulkan_pipeline.cpp
      vulkan_command_buffer.h
      vulkan_command_buffer.cpp
      vulkan_swapchain.h
      vulkan_swapchain.cpp
    pipeline_cache.h
    pipeline_cache.cpp
  tests/
    CMakeLists.txt
    test_rhi_types.cpp
    test_vulkan_device.cpp
    test_vulkan_resources.cpp
    test_rhi_integration.cpp
```

## Dependencies on other plans

| Dependency | What it provides | Impact if not ready |
|---|---|---|
| Plan 1 (ECS types) | `helios-core` with World, Entity, logging | Link target needed. If logging macros not ready, use `spdlog` directly. |
| Plan 3 (Window) | GLFW window + `VkSurfaceKHR` | Swapchain tests require a real surface. Without Plan 3, swapchain tests are skipped or mocked with `VK_NULL_HANDLE`. All other tests work headless. |
