# Rendering

## Overview

The rendering stack is split into three layers:

1. **RHI** (`helios/rhi/`) -- abstract GPU interface.
2. **Vulkan backend** (`helios/vulkan/`) -- Vulkan implementation of the RHI.
3. **Forward+ pipeline** (`helios/forward_plus/`) -- the default render
   pipeline built on top of the RHI.

Raw Vulkan types never appear outside `helios/vulkan/`. All rendering code
above that layer uses RHI abstractions.

## RHI Abstraction

The RHI lives in `helios::rhi` and defines an abstract `Device` plus a set
of GPU resource interfaces.

### Device

The `Device` is the primary factory. All GPU objects are created through it:

```cpp
class Device {
public:
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 3;

    virtual std::unique_ptr<Texture> create_texture(const TextureDesc&, const void* data = nullptr) = 0;
    virtual std::unique_ptr<Buffer>  create_buffer(const BufferDesc&, const void* data = nullptr) = 0;
    virtual std::unique_ptr<Shader>  create_shader(const ShaderDesc&) = 0;
    virtual std::unique_ptr<Pipeline> create_graphics_pipeline(const GraphicsPipelineDesc&) = 0;
    virtual std::unique_ptr<Pipeline> create_compute_pipeline(const ComputePipelineDesc&) = 0;
    virtual std::unique_ptr<CommandBuffer> create_command_buffer() = 0;
    virtual std::unique_ptr<Swapchain> create_swapchain(const SwapchainDesc&) = 0;
    virtual std::unique_ptr<RenderPass> create_render_pass(const RenderPassDesc&) = 0;
    virtual std::unique_ptr<Framebuffer> create_framebuffer(const FramebufferDesc&) = 0;
    virtual std::unique_ptr<DescriptorSetLayout> create_descriptor_set_layout(const DescriptorSetLayoutDesc&) = 0;
    virtual std::unique_ptr<DescriptorSet> allocate_descriptor_set(const DescriptorSetLayout&) = 0;
    virtual void update_descriptor_set(DescriptorSet&, const std::vector<DescriptorWrite>&) = 0;
    virtual void submit(const CommandBuffer&, const SubmitInfo& = {}) = 0;
    virtual void wait_idle() = 0;
};
```

All resources are returned as `std::unique_ptr`. Ownership is explicit.

### Deferred deletion

GPU resources cannot be destroyed immediately -- they may still be in-flight.
Use `Device::defer_destroy()`:

```cpp
device->defer_destroy(std::move(old_texture));
```

The resource is destroyed `MAX_FRAMES_IN_FLIGHT` frames later, when the GPU
is guaranteed to be done with it.

### Key resource types

| Type | Description |
|---|---|
| `Texture` | GPU image (2D, cube, array). Created from `TextureDesc`. |
| `Buffer` | GPU buffer (vertex, index, uniform, storage). Created from `BufferDesc`. |
| `Shader` | Compiled SPIR-V shader module. |
| `Pipeline` | Graphics or compute pipeline state object. |
| `CommandBuffer` | Records GPU commands for submission. |
| `Swapchain` | Manages presentation images. |
| `RenderPass` | Describes attachment load/store ops. |
| `Framebuffer` | Binds textures to a render pass. |
| `DescriptorSetLayout` / `DescriptorSet` | Shader resource binding. |

### Descriptor structs

```cpp
struct TextureDesc {
    uint32_t width, height;
    TextureFormat format;     // RGBA8, RGBA16F, Depth32F, etc.
    TextureType type;         // Texture2D, TextureCube, Texture2DArray
    uint32_t mip_levels;
    TextureUsage usage;       // Sampled | ColorAttachment | DepthAttachment | ...
    SamplerMode sampler;      // Repeat, ClampToEdge, MirroredRepeat
};

struct BufferDesc {
    uint32_t size;
    BufferUsage usage;        // Vertex | Index | Uniform | Storage | Transfer
    MemoryAccess access;      // GPU_Only, CPU_to_GPU, GPU_to_CPU
};

struct GraphicsPipelineDesc {
    const Shader* vertex_shader;
    const Shader* fragment_shader;
    VertexLayout layout;
    RenderState state;        // cull, depth, blend
    const RenderPass* render_pass;
    std::vector<const DescriptorSetLayout*> descriptor_layouts;
    uint32_t push_constant_size;
    // Dynamic rendering support (Vulkan 1.3)
    bool use_dynamic_rendering;
    std::vector<TextureFormat> dynamic_color_formats;
    TextureFormat dynamic_depth_format;
};
```

### GPU enumeration

```cpp
auto gpus = rhi::enumerate_devices(rhi::Backend::Vulkan);
for (auto& gpu : gpus) {
    // gpu.name, gpu.type, gpu.vram_bytes, gpu.driver_version
}

auto device = rhi::create_device(
    rhi::Backend::Vulkan,
    "My App",
    glfw_window,
    gpu_index,         // UINT32_MAX = auto-select best
    enable_validation
);
```

## Vulkan Backend

The Vulkan backend implements every RHI interface. Key classes:

| RHI Interface | Vulkan Implementation |
|---|---|
| `Device` | `VulkanDevice` |
| `Texture` | `VulkanTexture` |
| `Buffer` | `VulkanBuffer` |
| `Shader` | `VulkanShader` |
| `Pipeline` | `VulkanPipeline` |
| `CommandBuffer` | `VulkanCommandBuffer` |
| `Swapchain` | `VulkanSwapchain` |
| `RenderPass` | `VulkanRenderPass` |
| `Framebuffer` | `VulkanFramebuffer` |
| `DescriptorSetLayout` | `VulkanDescriptorSetLayout` |
| `DescriptorSet` | `VulkanDescriptorSet` |

`VulkanContext` manages the `VkInstance`, debug messenger, and physical device
selection. `VulkanDevice` owns the `VkDevice` and `VmaAllocator`.

Memory is managed through VMA (Vulkan Memory Allocator).

## RenderPlugin

The `RenderPlugin` sets up the rendering infrastructure:

```cpp
app.add_plugin(RenderPlugin{
    .backend = rhi::Backend::Vulkan,
    .app_name = "My Game",
    .enable_validation = true,
    .mode = RenderMode::Direct,           // blit to swapchain
    .initial_present_mode = rhi::PresentMode::Fifo,  // vsync
});
```

It inserts:
- `RenderContext` resource (device, swapchain, command buffers, scene
  framebuffers).
- `RenderSettings` resource (quality, debug flags, tonemap).
- `frame_begin` / `frame_end` / `frame_present` systems in `PreRender`.

### RenderMode

| Mode | Behavior |
|---|---|
| `Direct` | Scene framebuffer is blitted to the swapchain each frame |
| `Offscreen` | Scene framebuffer is left for external consumers (editor) |

## RenderContext

The `RenderContext` resource holds all GPU state:

```cpp
struct RenderContext {
    std::unique_ptr<rhi::Device> device;
    std::unique_ptr<rhi::Swapchain> swapchain;
    std::unique_ptr<rhi::CommandBuffer> cmds[MAX_FRAMES_IN_FLIGHT];
    rhi::CommandBuffer* cmd;          // current frame's command buffer

    // Triple-buffered scene framebuffers
    SceneFramebuffer scene_fbs[MAX_FRAMES_IN_FLIGHT];
    rhi::Texture* scene_color;        // current frame's color target
    rhi::Texture* scene_depth;        // current frame's depth target

    // Per-camera render targets
    std::unordered_map<uint32_t, CameraTarget> camera_targets;
};
```

### Per-camera render targets

Each camera can render to its own target:

```cpp
auto [color, depth] = ctx.get_or_create_target(camera_id, width, height);
```

Targets are lazily created and resized on demand. Cameras without a custom
target render to the shared scene framebuffer.

### Resizing

```cpp
ctx.resize(new_width, new_height, present_mode, resolution_scale);
ctx.resize_scene_fb(width, height);
```

## RenderSettings

Runtime-changeable settings stored as a world resource:

```cpp
struct RenderSettings {
    rhi::PresentMode present_mode;     // Immediate, Fifo, Mailbox
    float resolution_scale;            // 0.25 - 2.0
    glm::vec4 clear_color;
    float exposure;
    glm::vec3 ambient_color;
    float ambient_intensity;
    TonemapMode tonemap;               // None, Reinhard, ACES, Filmic
    uint32_t shadow_resolution;
    uint32_t shadow_cascades;
    DebugDraw debug_draw;              // Wireframe, Colliders, LightBounds, etc.
};

// Convenience
settings.set_vsync(true);
settings.set_present_mode(rhi::PresentMode::Mailbox);
settings.toggle_debug(DebugDraw::Wireframe);
```

Changes set dirty flags (`RenderDirty::Swapchain`, `RenderDirty::Quality`)
which `frame_begin` reacts to.

## Forward+ Pipeline

The default render pipeline. Registered via:

```cpp
app.add_plugin(ForwardPlusPlugin{});
```

### Pipeline stages

The Forward+ pipeline runs during `PreRender` in three phases:

**1. Extract** (`extract_render_data`)

Queries `Transform + MeshRenderer`, `Transform + PointLight`,
`Transform + DirectionalLight`, `Transform + Camera + ActiveCamera` and
populates a `FramePacket` resource with all render data for the frame.

**2. Camera driver** (`camera_driver`)

Iterates cameras in the `FramePacket` sorted by `order`. For each camera:
- Sets up viewport from camera settings.
- Selects the render schedule (defaults to Forward+).
- Invokes the schedule's draw function.

**3. Draw** (`forward_plus_draw_view`)

For each camera view, executes the render passes:

| Pass | Description |
|---|---|
| **Depth prepass** | Writes depth buffer, no color output |
| **Shadow pass** | Renders shadow maps for directional lights (cascaded) |
| **Light culling** | Compute shader: tiles the screen, culls point lights per tile |
| **Forward pass** | PBR shading with tiled light lists |
| **Skybox pass** | Renders environment cubemap behind geometry |
| **Tonemap pass** | HDR to LDR conversion (ACES, Reinhard, Filmic) |

### Render schedule registry

Cameras specify which pipeline to use via a string name:

```cpp
Camera cam;
cam.render_schedule = "forward_plus";  // resolved to a label at extract time
```

The `RenderScheduleRegistry` maps names to interned `RenderScheduleLabel`
values. Empty string means the default pipeline.

## GPU Resource Cache

`GPUResourceCache` lazily uploads CPU-side assets to the GPU:

```cpp
const GPUMesh* mesh = cache.get_or_upload_mesh(handle, mesh_asset, *device);
rhi::Texture* tex = cache.get_or_upload_texture(handle, tex_asset, *device);
const GPUMaterial* mat = cache.get_or_upload_material(handle, mat_asset, ...);
```

Features:
- **Deduplication**: same asset handle returns the cached GPU resource.
- **Eviction**: `cache.evict_unused(server, device)` removes entries whose
  asset handles are no longer valid in the `AssetServer`.
- **Default textures**: 1x1 white, blue (flat normal), and black fallback
  textures are created on first use.

### Pipeline cache

`PipelineCache` caches compiled `rhi::Pipeline` objects, keyed by a
configuration hash. Avoids redundant pipeline compilation when multiple
objects share the same material/render state.

## Render Layers

Entities and cameras have a `RenderLayers` bitmask. A camera only renders
entities whose layer mask intersects with the camera's mask:

```cpp
RenderLayers cam_layers = RenderLayers::layer(0).with(1);  // layers 0 + 1
RenderLayers entity_layers = RenderLayers::layer(1);        // layer 1 only
cam_layers.intersects(entity_layers);  // true
```

Entities and cameras without a `RenderLayers` component default to layer 0.
