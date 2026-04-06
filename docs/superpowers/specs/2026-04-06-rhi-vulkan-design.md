# Helios Rendering: RHI Abstraction + Vulkan Backend

## Goal

Replace all OpenGL rendering code with a Rendering Hardware Interface (RHI) abstraction layer backed by a Vulkan implementation. Full OpenGL removal. RHI designed so DirectX 12 can be added later as a second backend.

## Decisions

- **Backend:** Vulkan only (OpenGL fully removed)
- **Helper libraries:** VMA (memory allocator) + vk-bootstrap (instance/device setup)
- **Shader pipeline:** GLSL → SPIR-V via glslc at build time. SPIRV-Cross for future DX12 cross-compilation.
- **Rendering pipeline:** Keep Forward+ (depth prepass → compute light culling → forward shading → HDR tonemap). RHI designed to support deferred later.
- **Migration:** Full replacement, not gradual. Engine won't render during transition.

## Architecture

### Layer Structure

```
Engine Code (Renderer, Scene, Material, Camera)
    │
    ▼
RHI Interfaces (Engine/src/RHI/)
    │  Pure virtual classes. No backend types.
    │  Descriptions (TextureDesc, BufferDesc, PipelineDesc) are plain structs.
    │  Handles (RHITexture, RHIBuffer, RHIPipeline) are opaque base classes.
    ▼
Vulkan Backend (Engine/src/Renderer/Vulkan/)
    │  Implements all RHI interfaces.
    │  Owns all VkImage, VkBuffer, VkPipeline etc.
    │  Uses VMA for memory, vk-bootstrap for init.
    ▼
Vulkan SDK + GLFW Surface
```

### RHI Interfaces

All in `Engine/src/RHI/`:

**RHIDevice** — GPU resource factory.
```cpp
class RHIDevice {
public:
    virtual ~RHIDevice() = default;
    virtual Ref<RHITexture> CreateTexture(const TextureDesc&, const void* initialData = nullptr) = 0;
    virtual Ref<RHIBuffer> CreateBuffer(const BufferDesc&, const void* initialData = nullptr) = 0;
    virtual Ref<RHIShader> CreateShader(const ShaderDesc&) = 0;
    virtual Ref<RHIPipeline> CreateGraphicsPipeline(const GraphicsPipelineDesc&) = 0;
    virtual Ref<RHIPipeline> CreateComputePipeline(const ComputePipelineDesc&) = 0;
    virtual Ref<RHIRenderPass> CreateRenderPass(const RenderPassDesc&) = 0;
    virtual Ref<RHIFramebuffer> CreateFramebuffer(const FramebufferDesc&) = 0;
    virtual Ref<RHIDescriptorSetLayout> CreateDescriptorSetLayout(const DescriptorSetLayoutDesc&) = 0;
    virtual Ref<RHIDescriptorSet> AllocateDescriptorSet(const RHIDescriptorSetLayout*) = 0;
    virtual void UpdateDescriptorSet(RHIDescriptorSet*, const std::vector<DescriptorWrite>&) = 0;
    virtual Ref<RHICommandBuffer> CreateCommandBuffer() = 0;
    virtual void SubmitCommandBuffer(RHICommandBuffer*) = 0;
    virtual void WaitIdle() = 0;
};
```

**RHICommandBuffer** — Records GPU commands.
```cpp
class RHICommandBuffer {
public:
    virtual ~RHICommandBuffer() = default;
    virtual void Begin() = 0;
    virtual void End() = 0;
    virtual void BeginRenderPass(RHIRenderPass*, RHIFramebuffer*, const ClearValues&) = 0;
    virtual void EndRenderPass() = 0;
    virtual void BindPipeline(RHIPipeline*) = 0;
    virtual void SetViewport(float x, float y, float width, float height) = 0;
    virtual void SetScissor(int32_t x, int32_t y, uint32_t width, uint32_t height) = 0;
    virtual void BindVertexBuffer(RHIBuffer*, uint32_t binding = 0) = 0;
    virtual void BindIndexBuffer(RHIBuffer*, IndexType type = IndexType::Uint32) = 0;
    virtual void BindDescriptorSet(uint32_t set, RHIDescriptorSet*) = 0;
    virtual void PushConstants(ShaderStage stage, uint32_t offset, uint32_t size, const void* data) = 0;
    virtual void Draw(uint32_t vertexCount, uint32_t instanceCount = 1) = 0;
    virtual void DrawIndexed(uint32_t indexCount, uint32_t instanceCount = 1) = 0;
    virtual void Dispatch(uint32_t x, uint32_t y, uint32_t z) = 0;
    virtual void PipelineBarrier(const BarrierDesc&) = 0;
    virtual void CopyBuffer(RHIBuffer* src, RHIBuffer* dst, uint32_t size) = 0;
};
```

**RHISwapchain** — Presentation.
```cpp
class RHISwapchain {
public:
    virtual ~RHISwapchain() = default;
    virtual bool AcquireNextImage() = 0;
    virtual void Present() = 0;
    virtual void Resize(uint32_t width, uint32_t height) = 0;
    virtual RHITexture* GetCurrentImage() = 0;
    virtual RHIRenderPass* GetRenderPass() = 0;
    virtual RHIFramebuffer* GetCurrentFramebuffer() = 0;
    virtual uint32_t GetWidth() const = 0;
    virtual uint32_t GetHeight() const = 0;
};
```

**Resource handles** — Opaque base classes.
```cpp
class RHITexture     { public: virtual ~RHITexture() = default; };
class RHIBuffer      { public: virtual ~RHIBuffer() = default;
                               virtual void SetData(const void* data, uint32_t size, uint32_t offset = 0) = 0;
                               virtual void* Map() = 0;
                               virtual void Unmap() = 0; };
class RHIShader      { public: virtual ~RHIShader() = default; };
class RHIPipeline    { public: virtual ~RHIPipeline() = default; };
class RHIRenderPass  { public: virtual ~RHIRenderPass() = default; };
class RHIFramebuffer { public: virtual ~RHIFramebuffer() = default;
                               virtual uint32_t GetWidth() const = 0;
                               virtual uint32_t GetHeight() const = 0; };
class RHIDescriptorSetLayout { public: virtual ~RHIDescriptorSetLayout() = default; };
class RHIDescriptorSet       { public: virtual ~RHIDescriptorSet() = default; };
```

### Resource Descriptions

All descriptions are plain structs in `Engine/src/RHI/RHITypes.h`:

```cpp
enum class ImageFormat { R8, RG8, RGB8, RGBA8, RG16F, RGBA16F, RGBA32F, DEPTH24_STENCIL8, DEPTH32F };
enum class TextureType { Texture2D, TextureCube, Texture2DArray };
enum class TextureUsage : uint32_t { Sampled = 1, ColorAttachment = 2, DepthAttachment = 4, Storage = 8, Transfer = 16 };
enum class BufferUsage : uint32_t { Vertex = 1, Index = 2, Uniform = 4, Storage = 8, Transfer = 16 };
enum class MemoryAccess { GPU_Only, CPU_to_GPU, GPU_to_CPU };
enum class ShaderStage : uint32_t { Vertex = 1, Fragment = 2, Compute = 4, Geometry = 8 };
enum class IndexType { Uint16, Uint32 };
enum class CullMode { None, Front, Back };
enum class DepthCompare { Less, LessEqual, Greater, GreaterEqual, Never, Always };
enum class BlendMode { None, Alpha, Additive };
enum class LoadOp { Load, Clear, DontCare };
enum class StoreOp { Store, DontCare };

struct TextureDesc { uint32_t Width, Height; ImageFormat Format; TextureType Type; uint32_t MipLevels, ArrayLayers, Samples; TextureUsage Usage; };
struct BufferDesc { uint32_t Size; BufferUsage Usage; MemoryAccess Access; };
struct ShaderDesc { ShaderStage Stage; std::vector<uint8_t> SpirVCode; std::string EntryPoint = "main"; };

struct VertexAttribute { uint32_t Location, Binding, Offset; ImageFormat Format; };
struct VertexLayout { std::vector<VertexAttribute> Attributes; uint32_t Stride; };
struct RenderState { CullMode Cull = CullMode::Back; DepthCompare Depth = DepthCompare::Less; bool DepthWrite = true; BlendMode Blend = BlendMode::None; };

struct AttachmentDesc { ImageFormat Format; uint32_t Samples; LoadOp Load; StoreOp Store; };
struct RenderPassDesc { std::vector<AttachmentDesc> ColorAttachments; AttachmentDesc DepthAttachment; bool HasDepth; };
struct FramebufferDesc { RHIRenderPass* RenderPass; std::vector<RHITexture*> Attachments; uint32_t Width, Height; };

struct GraphicsPipelineDesc { RHIShader* VertexShader; RHIShader* FragmentShader; VertexLayout Layout; RenderState State; RHIRenderPass* RenderPass; std::vector<RHIDescriptorSetLayout*> DescriptorLayouts; uint32_t PushConstantSize; };
struct ComputePipelineDesc { RHIShader* ComputeShader; std::vector<RHIDescriptorSetLayout*> DescriptorLayouts; uint32_t PushConstantSize; };

struct DescriptorBinding { uint32_t Binding; DescriptorType Type; ShaderStage Stage; uint32_t Count; };
struct DescriptorSetLayoutDesc { std::vector<DescriptorBinding> Bindings; };
enum class DescriptorType { UniformBuffer, StorageBuffer, CombinedImageSampler, StorageImage };
struct DescriptorWrite { uint32_t Binding; DescriptorType Type; RHIBuffer* Buffer; RHITexture* Texture; uint32_t Offset, Range; };

struct ClearValues { glm::vec4 Color = {0,0,0,1}; float Depth = 1.0f; uint32_t Stencil = 0; };
struct BarrierDesc { ShaderStage SrcStage, DstStage; };
```

### Vulkan Backend Structure

```
Engine/src/Renderer/Vulkan/
├── VulkanDevice.h/cpp            — VkInstance, VkPhysicalDevice, VkDevice, VmaAllocator
├── VulkanSwapchain.h/cpp         — VkSwapchainKHR, frame-in-flight sync, image acquisition
├── VulkanCommandBuffer.h/cpp     — VkCommandBuffer + VkCommandPool
├── VulkanTexture.h/cpp           — VkImage + VkImageView + VkSampler + VmaAllocation
├── VulkanBuffer.h/cpp            — VkBuffer + VmaAllocation
├── VulkanShader.h/cpp            — VkShaderModule from SPIR-V bytes
├── VulkanPipeline.h/cpp          — VkPipeline + VkPipelineLayout
├── VulkanRenderPass.h/cpp        — VkRenderPass
├── VulkanFramebuffer.h/cpp       — VkFramebuffer
├── VulkanDescriptor.h/cpp        — VkDescriptorSetLayout + VkDescriptorPool + VkDescriptorSet
└── VulkanContext.h/cpp           — Instance creation, validation layers, debug messenger
```

**Vendored dependencies:**
- `Engine/vendor/vma/` — Vulkan Memory Allocator (single header `vk_mem_alloc.h`)
- `Engine/vendor/vk-bootstrap/` — vk-bootstrap (header + cpp)
- Vulkan SDK headers from system install

### Shader Pipeline

**Source:** GLSL files in `Editor/Resources/Shaders/` (existing)

**Build step:** Pre-build command compiles all GLSL to SPIR-V:
```bash
for f in Editor/Resources/Shaders/*.vert Editor/Resources/Shaders/*.frag Editor/Resources/Shaders/*.comp; do
    glslc "$f" -o "$f.spv"
done
```

**GLSL changes needed for Vulkan:**
- Add `#version 450` (already present or easy to add)
- Replace `uniform` blocks with `layout(set=N, binding=M) uniform UBO { ... }`
- Replace `uniform sampler2D` with `layout(set=N, binding=M) uniform sampler2D`
- Replace SSBO syntax to use `layout(set=N, binding=M) buffer` 
- Remove `layout(location=X)` for uniforms (not applicable in Vulkan — use descriptors)

### Material System Changes

**Current:** Material = ShaderProgram + map of textures + map of uniforms. Calls `glUniform*` per draw.

**New:** Material = Pipeline reference + DescriptorSet(s).

```cpp
class Material {
    Ref<RHIPipeline> m_Pipeline;           // Baked shader + render state
    Ref<RHIDescriptorSet> m_DescriptorSet; // Textures + UBO data
    Ref<RHIBuffer> m_UniformBuffer;        // Per-material UBO

    void SetTexture(uint32_t binding, Ref<RHITexture> texture);
    void SetUniform(const std::string& name, const void* data, uint32_t size);
    void Bind(RHICommandBuffer* cmd);      // Binds pipeline + descriptor set
};
```

### Renderer Changes

The `Renderer` class keeps its Forward+ structure but records to command buffers:

```
Renderer::BeginDrawing():
    cmd = device->CreateCommandBuffer()
    cmd->Begin()

    // Depth prepass
    cmd->BeginRenderPass(depthPass, depthFB, clearDepth)
    cmd->BindPipeline(depthPipeline)
    for mesh: BindVertexBuffer, BindIndexBuffer, DrawIndexed
    cmd->EndRenderPass()

    // Shadow pass
    cmd->BeginRenderPass(shadowPass, shadowFB, clearDepth)
    cmd->BindPipeline(shadowPipeline)
    for mesh: DrawIndexed
    cmd->EndRenderPass()

    // Light culling (compute)
    cmd->BindPipeline(lightCullPipeline)
    cmd->BindDescriptorSet(0, lightCullDescriptors)
    cmd->Dispatch(workgroupsX, workgroupsY, 1)
    cmd->PipelineBarrier(compute→fragment)

    // Forward shading
    cmd->BeginRenderPass(hdrPass, hdrFB, clearColor)
    for mesh:
        cmd->BindPipeline(material.pipeline)
        cmd->BindDescriptorSet(0, globalDescriptors)  // camera, lights
        cmd->BindDescriptorSet(1, material.descriptors) // textures, material UBO
        cmd->BindVertexBuffer(mesh.vbo)
        cmd->BindIndexBuffer(mesh.ibo)
        cmd->DrawIndexed(indexCount)
    cmd->EndRenderPass()

    // Tonemap
    cmd->BeginRenderPass(tonemapPass, swapchainFB, clearColor)
    cmd->BindPipeline(tonemapPipeline)
    cmd->BindDescriptorSet(0, hdrDescriptor)
    cmd->Draw(3)  // fullscreen triangle
    cmd->EndRenderPass()

    cmd->End()
    device->SubmitCommandBuffer(cmd)
    swapchain->Present()
```

### File Changes

**New files:**
| Directory | Files |
|-----------|-------|
| `Engine/src/RHI/` | `RHIDevice.h`, `RHICommandBuffer.h`, `RHISwapchain.h`, `RHIResources.h`, `RHITypes.h`, `RHIDescriptor.h` |
| `Engine/src/Renderer/Vulkan/` | `VulkanDevice.h/cpp`, `VulkanSwapchain.h/cpp`, `VulkanCommandBuffer.h/cpp`, `VulkanTexture.h/cpp`, `VulkanBuffer.h/cpp`, `VulkanShader.h/cpp`, `VulkanPipeline.h/cpp`, `VulkanRenderPass.h/cpp`, `VulkanFramebuffer.h/cpp`, `VulkanDescriptor.h/cpp`, `VulkanContext.h/cpp` |
| `Engine/vendor/vma/` | `vk_mem_alloc.h` |
| `Engine/vendor/vk-bootstrap/` | `VkBootstrap.h`, `VkBootstrap.cpp` |

**Deleted files:**
| File | Reason |
|------|--------|
| `Engine/src/Renderer/RendererAPI.h/cpp` | Replaced by RHI |
| `Engine/src/Renderer/RenderContext.h/cpp` | Replaced by VulkanContext + VulkanSwapchain |
| `Engine/src/Renderer/Shader.h/cpp` | Replaced by VulkanShader (SPIR-V) |
| `Engine/src/Renderer/ShaderProgram.h/cpp` | Replaced by RHIPipeline |
| `Engine/src/Renderer/VertexArray.h/cpp` | No VAO in Vulkan — vertex input is part of pipeline |
| `Engine/src/Renderer/UniformBuffer.h/cpp` | Replaced by RHIBuffer with Uniform usage |
| `Engine/vendor/Glad/` | OpenGL loader no longer needed |

**Rewritten files:**
| File | Change |
|------|--------|
| `Engine/src/Renderer/Renderer.h/cpp` | Record to RHICommandBuffer instead of GL calls |
| `Engine/src/Renderer/Framebuffer.h/cpp` | Becomes thin wrapper around RHIRenderPass + RHIFramebuffer |
| `Engine/src/Renderer/Texture.h/cpp` | Becomes thin wrapper around RHITexture |
| `Engine/src/Renderer/Buffer.h/cpp` | Becomes thin wrapper around RHIBuffer |
| `Engine/src/Renderer/Material.h/cpp` | Pipeline ref + DescriptorSet instead of uniform map |

**Modified files:**
| File | Change |
|------|--------|
| `Dependencies.lua` | Remove Glad, add Vulkan SDK include path, add VMA/vk-bootstrap |
| `Engine/premake5.lua` | Remove Glad links, add vulkan lib, add VMA/vk-bootstrap includes and source |
| `premake5.lua` | Remove Glad from dependency group |
| `Editor/premake5.lua` | Remove Glad links |
| `EditorLauncher/premake5.lua` | Remove Glad links |
| `Engine/src/pch.h` | Remove glad include |
| `Engine/src/ImGui/ImGuiLayer.cpp` | Switch from `ImGui_ImplOpenGL3` to `ImGui_ImplVulkan` |
| `Engine/src/ImGui/imgui_impl_opengl3.cpp/h` | Delete, replace with `imgui_impl_vulkan.cpp/h` |
| `Engine/src/Platform/*/Window.cpp` | Create Vulkan surface instead of GL context |
| All GLSL shaders | Add Vulkan-compatible descriptor layout declarations |

### Migration Phases

**Phase 1: RHI interfaces + Vulkan init (get a triangle)**
- Create all RHI interface headers
- Implement VulkanDevice, VulkanSwapchain, VulkanCommandBuffer
- Implement VulkanBuffer, VulkanShader, VulkanPipeline, VulkanRenderPass
- Render a hardcoded triangle to verify the pipeline works
- Vendor VMA + vk-bootstrap

**Phase 2: Port Renderer to RHI**
- Implement VulkanTexture, VulkanFramebuffer, VulkanDescriptor
- Rewrite Renderer.cpp to record commands via RHI
- Port each pass: depth → shadow → light culling → forward → tonemap
- Port Material to use pipelines + descriptor sets
- Convert GLSL shaders to Vulkan-compatible GLSL + compile to SPIR-V

**Phase 3: Delete OpenGL + integrate**
- Remove RendererAPI, RenderContext, Shader, ShaderProgram, VertexArray, UniformBuffer
- Remove Glad vendor
- Switch ImGui to Vulkan backend
- Switch GLFW window to Vulkan surface
- Update build system
- Full integration test
