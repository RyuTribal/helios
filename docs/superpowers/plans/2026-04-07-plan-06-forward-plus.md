# Plan 6: ForwardPlus Rendering Pipeline — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement the complete Forward+ rendering pipeline as a set of render graph nodes, porting all existing render pass logic from `Engine/src/Renderer/RenderPasses.cpp` and `Engine/src/Renderer/Renderer.cpp` into the new architecture. Each pass becomes a self-contained graph node that declares its resource dependencies and executes independently.

**Architecture:** The `ForwardPlusPlugin` registers two systems: `extract_render_data` (ECS queries to FramePacket) and `build_forward_plus_graph` (FramePacket to RenderGraph). Each pass (depth prepass, shadows, light culling, forward, skybox, tonemap) lives in its own file and uses the `RenderGraph::add_pass()` API. IBL generation runs as compute dispatches on the render thread. DefaultTextures and PipelineCache are RAII resources. Shaders are compiled at build time via a CMake function.

**Tech Stack:** C++20, Vulkan 1.3 (via RHI from Plan 2), glslc (SPIR-V), glm, CMake

**Spec:** `docs/superpowers/specs/2026-04-07-engine-rewrite-design.md` sections 4.3, 4.5, 4.6

**Dependencies (Plans 1-5):** ECS World, App/Plugin/Scheduler, RHI (Device, Texture, Buffer, Pipeline, CommandBuffer, DescriptorSet), RenderGraph/RenderGraphBuilder/RenderContext, RenderThread, FramePacket

**Key design changes from current code:**
- **No singletons.** Current `Renderer::s_Instance`, `DefaultTextures::s_*` statics are replaced by RAII resources owned by the World or render thread.
- **No Init/Shutdown.** All objects construct in constructors and destroy in destructors.
- **No Ref<T>.** Use `rhi::Texture`, `rhi::Buffer`, etc. as move-only RAII value types. Use `std::unique_ptr` only for polymorphism.
- **No raw Vulkan calls in pass code.** Passes use `RenderContext::cmd()` which returns the RHI `CommandBuffer`. Layout transitions are handled automatically by the render graph's barrier insertion.
- **Passes are graph nodes, not function calls.** Each pass declares reads/writes/creates in a setup lambda, and records GPU commands in an execute lambda. The graph handles ordering, barriers, and resource lifetime.

**Source files (all under `helios-renderer/src/forward_plus/`):**

| File | Purpose |
|------|---------|
| `forward_plus_plugin.h/.cpp` | Plugin struct, config, extract and graph-build systems |
| `depth_prepass.h/.cpp` | Depth-only render pass graph node |
| `shadow_pass.h/.cpp` | Cascade shadow map generation graph node |
| `light_culling.h/.cpp` | Tile-based Forward+ compute dispatch graph node |
| `forward_pass.h/.cpp` | PBR forward rendering graph node |
| `skybox_pass.h/.cpp` | Skybox rendering graph node |
| `tonemap_pass.h/.cpp` | Exposure tonemapping compute dispatch graph node |
| `equirect_to_cube.h/.cpp` | Equirectangular-to-cubemap conversion compute |
| `ibl_generation.h/.cpp` | Irradiance convolution, prefilter, BRDF LUT compute |

**Additional files:**

| File | Purpose |
|------|---------|
| `helios-renderer/src/default_textures.h/.cpp` | RAII default textures (white, black, blue, black cube, etc.) |
| `helios-renderer/src/pipeline_cache.h/.cpp` | Shader loading and pipeline caching |
| `cmake/compile_shaders.cmake` | CMake function for GLSL-to-SPIR-V compilation |
| `shaders/*.vert/*.frag/*.comp/*.geom` | Ported shader sources at project root |

---

## Task 1: ForwardPlusConfig Resource and Plugin Shell

**Files:**
- Create: `helios-renderer/src/forward_plus/forward_plus_plugin.h`
- Create: `helios-renderer/src/forward_plus/forward_plus_plugin.cpp`

- [ ] **Step 1: Define ForwardPlusConfig**

```cpp
// forward_plus_plugin.h
#pragma once

#include "helios-core/src/app/plugin.h"
#include "helios-renderer/src/rhi/rhi_types.h"

namespace helios {

struct ForwardPlusConfig {
    TextureFormat hdr_format = TextureFormat::RGBA16F;
    uint32_t shadow_resolution = 4096;
    uint32_t shadow_cascades = 4;
    TextureFormat depth_format = TextureFormat::Depth32F;
    float exposure = 1.0f;

    // Cascade split distances (computed from camera far plane)
    // If empty, auto-computed as geometric splits
    std::vector<float> cascade_splits{};

    // IBL settings
    uint32_t irradiance_resolution = 32;
    uint32_t prefilter_resolution = 128;
    uint32_t brdf_lut_resolution = 512;

    // Tile size for light culling compute shader
    uint32_t tile_size = 16;

    // Max light counts
    uint32_t max_point_lights = 1024;
    uint32_t max_dir_lights = 4;
};

// Forward declarations for systems
void extract_render_data(/* system params */);
void build_forward_plus_graph(/* system params */);

struct ForwardPlusPlugin {
    ForwardPlusConfig config{};

    void build(App& app);
};

} // namespace helios
```

- [ ] **Step 2: Implement the plugin build method**

```cpp
// forward_plus_plugin.cpp
#include "forward_plus_plugin.h"
#include "helios-renderer/src/frame_packet.h"

namespace helios {

void ForwardPlusPlugin::build(App& app) {
    // Ensure RHI plugin is present (VulkanRenderPlugin adds Device, Swapchain, RenderGraph, RenderThread)
    // ForwardPlusPlugin depends on those existing as resources

    app.insert_resource(config);
    app.add_system(Schedule::PreRender, extract_render_data);
    app.add_system(Schedule::PreRender, build_forward_plus_graph
        .after(extract_render_data));
}

} // namespace helios
```

- [ ] **Step 3: Verify plugin compiles and can be registered with App**

---

## Task 2: FramePacket Data Structures

**Files:**
- Create: `helios-renderer/src/frame_packet.h`

This file defines the data that flows from the main thread (ECS) to the render thread.

- [ ] **Step 1: Define GPU-facing data structs and FramePacket**

```cpp
// frame_packet.h
#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>
#include "helios-core/src/ecs/components.h"  // AssetHandle

namespace helios {

struct CameraData {
    glm::mat4 view{1.0f};
    glm::mat4 projection{1.0f};
    glm::vec3 position{0.0f};
    float near_plane = 0.1f;
    float far_plane = 500.0f;
    float fov_y = 45.0f;
    float aspect_ratio = 16.0f / 9.0f;
};

struct MeshDraw {
    glm::mat4 transform{1.0f};
    AssetHandle mesh{};
    AssetHandle material{};
};

struct LightData {
    glm::vec3 position{0.0f};
    glm::vec3 color{1.0f};
    float intensity = 1.0f;
    float radius = 10.0f;
    float constant_attenuation = 1.0f;
    float linear_attenuation = 0.09f;
    float quadratic_attenuation = 0.032f;
};

struct DirLightData {
    glm::vec3 direction{0.0f, -1.0f, 0.0f};
    glm::vec3 color{1.0f};
    float intensity = 1.0f;
    bool cast_shadows = true;
};

struct SkyboxData {
    AssetHandle environment_map{};
    float brightness = 1.0f;
};

struct FramePacket {
    CameraData camera;
    std::vector<MeshDraw> mesh_draws;
    std::vector<LightData> point_lights;
    std::vector<DirLightData> dir_lights;
    SkyboxData skybox;
    uint32_t viewport_width = 1920;
    uint32_t viewport_height = 1080;

    void clear() {
        mesh_draws.clear();
        point_lights.clear();
        dir_lights.clear();
        skybox = {};
    }
};

} // namespace helios
```

- [ ] **Step 2: Verify FramePacket is trivially moveable and can be submitted to RenderThread**

---

## Task 3: extract_render_data System

**Files:**
- Modify: `helios-renderer/src/forward_plus/forward_plus_plugin.h`
- Modify: `helios-renderer/src/forward_plus/forward_plus_plugin.cpp`

- [ ] **Step 1: Implement extract_render_data as a free function system**

This system runs on the main thread during `PreRender`. It queries the ECS for all visible entities with render-relevant components and packs them into a `FramePacket` resource.

```cpp
// In forward_plus_plugin.cpp

void extract_render_data(
    Query<const Transform, const MeshRenderer, Without<Disabled>> meshes,
    Query<const Transform, const PointLight> point_lights,
    Query<const Transform, const DirectionalLight> dir_lights,
    Query<const Transform, const Camera, With<ActiveCamera>> camera,
    ResMut<FramePacket> packet
) {
    packet->clear();

    // Extract active camera
    for (auto [t, cam] : camera) {
        packet->camera = CameraData{
            .view = glm::inverse(t.to_mat4()),
            .projection = cam.projection_matrix(),
            .position = t.position,
            .near_plane = cam.near_plane,
            .far_plane = cam.far_plane,
            .fov_y = cam.fov_y,
            .aspect_ratio = cam.aspect_ratio(),
        };
        break; // only one active camera
    }

    // Extract mesh draws
    packet->mesh_draws.reserve(meshes.count());
    for (auto [t, mr] : meshes) {
        packet->mesh_draws.push_back(MeshDraw{
            .transform = t.to_mat4(),
            .mesh = mr.mesh,
            .material = mr.material,
        });
    }

    // Extract point lights
    packet->point_lights.reserve(point_lights.count());
    for (auto [t, pl] : point_lights) {
        packet->point_lights.push_back(LightData{
            .position = t.position,
            .color = pl.color,
            .intensity = pl.intensity,
            .radius = pl.radius,
        });
    }

    // Extract directional lights
    for (auto [t, dl] : dir_lights) {
        packet->dir_lights.push_back(DirLightData{
            .direction = dl.direction,
            .color = dl.color,
            .intensity = dl.intensity,
            .cast_shadows = dl.cast_shadows,
        });
    }
}
```

- [ ] **Step 2: Verify system signature matches ECS query/resource types from Plan 1**

---

## Task 4: GPU Data Structures (Shader-Matching UBO/SSBO Layouts)

**Files:**
- Create: `helios-renderer/src/forward_plus/gpu_data.h`

These structs are shared between C++ and GLSL via matching `std140`/`std430` layouts.

- [ ] **Step 1: Define all GPU-facing structs**

```cpp
// gpu_data.h
#pragma once

#include <glm/glm.hpp>

namespace helios {

// Matches GlobalUBO in default_static_shader.vert (set 0, binding 0)
struct alignas(16) GlobalUBOData {
    glm::mat4 camera_view;
    glm::mat4 camera_projection;
    glm::vec3 camera_pos;
    float camera_far_plane;
    int num_directional_lights;
    int num_tiles_x;
    float environment_brightness;
    float _padding;
};
static_assert(sizeof(GlobalUBOData) == 160);

// Matches PushConstants in depth_pre_pass.vert, default_static_shader.vert
struct PushConstantData {
    glm::mat4 transform;
};

// Matches PointLightInfo in light_culling_shader.comp and default_static_shader.frag
struct alignas(16) PointLightGPU {
    float constant_attenuation;
    float linear_attenuation;
    float quadratic_attenuation;
    float intensity;
    glm::vec4 color;
    glm::vec4 position;
};

// Matches DirectionalLightInfo in default_static_shader.frag
struct alignas(16) DirLightGPU {
    glm::vec3 _padding;
    float intensity;
    glm::vec4 color;
    glm::vec4 direction;
};

// Matches VisibleIndex in light_culling_shader.comp
struct VisibleIndex {
    int index;
};

// Matches LightCullingParams UBO in light_culling_shader.comp (set 0, binding 0)
struct alignas(16) LightCullingParams {
    glm::mat4 view;
    glm::mat4 projection;
    glm::ivec2 screen_size;
    int light_count;
    int _pad;
};

// Matches CameraUBO in depth_pre_pass.vert (set 0, binding 0)
struct CameraUBOData {
    glm::mat4 view;
    glm::mat4 projection;
};

// Matches SkyboxUBO in skybox.vert (set 0, binding 0)
struct SkyboxUBOData {
    glm::mat4 camera_view;
    glm::mat4 camera_projection;
    float brightness;
};

// Matches MaterialUBO in default_static_shader.frag (set 1, binding 0)
struct alignas(16) MaterialGPU {
    glm::vec3 albedo_color;
    float metalness;
    float roughness;
    float emission;
    int use_normal_map;
    float _padding;
};

// Cascade shadow push constants
struct ShadowPushConstant {
    glm::mat4 transform;
};

// Prefilter push constants (matches prefilter_envmap.comp)
struct PrefilterPushConstants {
    float roughness;
    uint32_t mip_size;
};

// Tonemap push constants (matches tonemap.comp)
struct TonemapPushConstants {
    float exposure;
};

// Constants
constexpr uint32_t MAX_POINT_LIGHTS = 1024;
constexpr uint32_t MAX_DIR_LIGHTS = 4;
constexpr uint32_t MAX_CASCADE_MATRICES = 16;
constexpr uint32_t TILE_SIZE = 16;
constexpr uint32_t MAX_VISIBLE_LIGHTS_PER_TILE = 1024;

} // namespace helios
```

- [ ] **Step 2: Verify all struct alignments match the GLSL std140/std430 layouts in existing shaders**

---

## Task 5: DefaultTextures as RAII Resource

**Files:**
- Create: `helios-renderer/src/default_textures.h`
- Create: `helios-renderer/src/default_textures.cpp`

Port from `Engine/src/Renderer/DefaultTextures.h/.cpp`. The key change: no static state, no `Init()`/`Shutdown()`. Constructor creates all textures, destructor releases them.

- [ ] **Step 1: Define DefaultTextures as a constructible resource**

```cpp
// default_textures.h
#pragma once

#include "rhi/rhi.h"

namespace helios {

// RAII resource — construct with a Device, textures are created in constructor,
// destroyed automatically when this object is destroyed.
// Inserted as a World resource by ForwardPlusPlugin.
class DefaultTextures {
public:
    explicit DefaultTextures(rhi::Device& device);
    ~DefaultTextures() = default;

    DefaultTextures(DefaultTextures&&) noexcept = default;
    DefaultTextures& operator=(DefaultTextures&&) noexcept = default;
    DefaultTextures(const DefaultTextures&) = delete;
    DefaultTextures& operator=(const DefaultTextures&) = delete;

    const rhi::Texture& white() const { return m_white; }
    const rhi::Texture& black() const { return m_black; }
    const rhi::Texture& blue() const { return m_blue; }        // normal-map default
    const rhi::Texture& black_cube() const { return m_black_cube; }
    const rhi::Texture& white_array() const { return m_white_array; }
    const rhi::Texture& brdf_lut_placeholder() const { return m_brdf_lut; }

private:
    rhi::Texture m_white;
    rhi::Texture m_black;
    rhi::Texture m_blue;
    rhi::Texture m_black_cube;
    rhi::Texture m_white_array;
    rhi::Texture m_brdf_lut;
};

} // namespace helios
```

- [ ] **Step 2: Implement the constructor**

```cpp
// default_textures.cpp
#include "default_textures.h"

namespace helios {

DefaultTextures::DefaultTextures(rhi::Device& device) {
    // 1x1 RGBA8 white
    {
        uint32_t data = 0xFFFFFFFF;
        TextureDesc desc{
            .width = 1, .height = 1,
            .format = TextureFormat::RGBA8,
            .type = TextureType::Texture2D,
            .mip_levels = 1,
            .usage = TextureUsage::Sampled | TextureUsage::Transfer,
            .debug_name = "DefaultWhite",
        };
        m_white = rhi::Texture(device, desc, &data);
    }

    // 1x1 RGBA8 black
    {
        uint32_t data = 0xFF000000;
        TextureDesc desc{
            .width = 1, .height = 1,
            .format = TextureFormat::RGBA8,
            .type = TextureType::Texture2D,
            .mip_levels = 1,
            .usage = TextureUsage::Sampled | TextureUsage::Transfer,
            .debug_name = "DefaultBlack",
        };
        m_black = rhi::Texture(device, desc, &data);
    }

    // 1x1 RGBA8 blue (flat normal: 128, 128, 255, 255)
    {
        uint32_t data = 0xFFFF8080;
        TextureDesc desc{
            .width = 1, .height = 1,
            .format = TextureFormat::RGBA8,
            .type = TextureType::Texture2D,
            .mip_levels = 1,
            .usage = TextureUsage::Sampled | TextureUsage::Transfer,
            .debug_name = "DefaultBlue",
        };
        m_blue = rhi::Texture(device, desc, &data);
    }

    // 1x1 black cubemap (6 faces)
    {
        uint32_t faces[6] = { 0xFF000000, 0xFF000000, 0xFF000000,
                              0xFF000000, 0xFF000000, 0xFF000000 };
        TextureDesc desc{
            .width = 1, .height = 1,
            .format = TextureFormat::RGBA8,
            .type = TextureType::TextureCube,
            .mip_levels = 1,
            .array_layers = 6,
            .usage = TextureUsage::Sampled | TextureUsage::Transfer,
            .debug_name = "DefaultBlackCube",
        };
        m_black_cube = rhi::Texture(device, desc, faces);
    }

    // 1x1 white 2D array (1 layer) for sampler2DArray bindings
    {
        uint32_t data = 0xFFFFFFFF;
        TextureDesc desc{
            .width = 1, .height = 1,
            .format = TextureFormat::RGBA8,
            .type = TextureType::Texture2DArray,
            .mip_levels = 1,
            .array_layers = 1,
            .usage = TextureUsage::Sampled | TextureUsage::Transfer,
            .debug_name = "DefaultWhiteArray",
        };
        m_white_array = rhi::Texture(device, desc, &data);
    }

    // 1x1 BRDF LUT placeholder: R=0.5, G=0.0 gives reasonable specular fallback
    {
        uint32_t data = 0xFF000080; // ABGR: R=128(0.5), G=0, B=0, A=255
        TextureDesc desc{
            .width = 1, .height = 1,
            .format = TextureFormat::RGBA8,
            .type = TextureType::Texture2D,
            .mip_levels = 1,
            .usage = TextureUsage::Sampled | TextureUsage::Transfer,
            .debug_name = "DefaultBRDF",
        };
        m_brdf_lut = rhi::Texture(device, desc, &data);
    }
}

} // namespace helios
```

- [ ] **Step 3: Verify DefaultTextures constructs and destructs cleanly; no Init/Shutdown**

---

## Task 6: PipelineCache as RAII Resource

**Files:**
- Create: `helios-renderer/src/pipeline_cache.h`
- Create: `helios-renderer/src/pipeline_cache.cpp`

Port from `Engine/src/Renderer/PipelineCache.h`. Same interface but without raw pointers or `Ref<T>`.

- [ ] **Step 1: Define PipelineCache**

```cpp
// pipeline_cache.h
#pragma once

#include "rhi/rhi.h"
#include <string>
#include <unordered_map>

namespace helios {

// Owns all loaded shaders and created pipelines.
// Inserted as a World resource (or owned by RenderThread).
class PipelineCache {
public:
    explicit PipelineCache(rhi::Device& device);
    ~PipelineCache() = default;

    PipelineCache(PipelineCache&&) noexcept = default;
    PipelineCache& operator=(PipelineCache&&) noexcept = default;
    PipelineCache(const PipelineCache&) = delete;
    PipelineCache& operator=(const PipelineCache&) = delete;

    // Load a shader from SPIR-V file. Returns reference to cached shader.
    // Returns nullptr if load fails.
    rhi::Shader* load_shader(const std::string& path, ShaderStage stage);

    // Get or create a graphics pipeline. Caches by name.
    rhi::Pipeline& get_or_create_graphics(const std::string& name,
                                          const GraphicsPipelineDesc& desc);

    // Get or create a compute pipeline. Caches by name.
    rhi::Pipeline& get_or_create_compute(const std::string& name,
                                         const ComputePipelineDesc& desc);

    // Lookup (returns nullptr if not found)
    rhi::Shader* get_shader(const std::string& path) const;
    rhi::Pipeline* get_graphics_pipeline(const std::string& name) const;
    rhi::Pipeline* get_compute_pipeline(const std::string& name) const;

    // Invalidate a pipeline (for shader hot-reload)
    void invalidate(const std::string& name);

private:
    rhi::Device& m_device;
    std::unordered_map<std::string, rhi::Shader> m_shaders;
    std::unordered_map<std::string, rhi::Pipeline> m_graphics_pipelines;
    std::unordered_map<std::string, rhi::Pipeline> m_compute_pipelines;
};

} // namespace helios
```

- [ ] **Step 2: Implement PipelineCache methods**

```cpp
// pipeline_cache.cpp
#include "pipeline_cache.h"
#include <fstream>

namespace helios {

PipelineCache::PipelineCache(rhi::Device& device)
    : m_device(device) {}

rhi::Shader* PipelineCache::load_shader(const std::string& path, ShaderStage stage) {
    auto it = m_shaders.find(path);
    if (it != m_shaders.end())
        return &it->second;

    // Read SPIR-V binary from file
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open())
        return nullptr;

    size_t size = file.tellg();
    std::vector<char> spirv(size);
    file.seekg(0);
    file.read(spirv.data(), size);

    ShaderDesc desc{
        .stage = stage,
        .spirv_data = spirv.data(),
        .spirv_size = size,
        .debug_name = path,
    };
    auto [inserted, success] = m_shaders.emplace(path, rhi::Shader(m_device, desc));
    if (!success)
        return nullptr;

    return &inserted->second;
}

rhi::Pipeline& PipelineCache::get_or_create_graphics(
    const std::string& name, const GraphicsPipelineDesc& desc)
{
    auto it = m_graphics_pipelines.find(name);
    if (it != m_graphics_pipelines.end())
        return it->second;

    auto [inserted, _] = m_graphics_pipelines.emplace(
        name, rhi::Pipeline(m_device, desc));
    return inserted->second;
}

rhi::Pipeline& PipelineCache::get_or_create_compute(
    const std::string& name, const ComputePipelineDesc& desc)
{
    auto it = m_compute_pipelines.find(name);
    if (it != m_compute_pipelines.end())
        return it->second;

    auto [inserted, _] = m_compute_pipelines.emplace(
        name, rhi::Pipeline(m_device, desc));
    return inserted->second;
}

rhi::Shader* PipelineCache::get_shader(const std::string& path) const {
    auto it = m_shaders.find(path);
    return it != m_shaders.end() ? const_cast<rhi::Shader*>(&it->second) : nullptr;
}

rhi::Pipeline* PipelineCache::get_graphics_pipeline(const std::string& name) const {
    auto it = m_graphics_pipelines.find(name);
    return it != m_graphics_pipelines.end()
        ? const_cast<rhi::Pipeline*>(&it->second) : nullptr;
}

rhi::Pipeline* PipelineCache::get_compute_pipeline(const std::string& name) const {
    auto it = m_compute_pipelines.find(name);
    return it != m_compute_pipelines.end()
        ? const_cast<rhi::Pipeline*>(&it->second) : nullptr;
}

void PipelineCache::invalidate(const std::string& name) {
    m_graphics_pipelines.erase(name);
    m_compute_pipelines.erase(name);
}

} // namespace helios
```

- [ ] **Step 3: Verify PipelineCache can load a shader and create a pipeline**

---

## Task 7: Depth Prepass Graph Node

**Files:**
- Create: `helios-renderer/src/forward_plus/depth_prepass.h`
- Create: `helios-renderer/src/forward_plus/depth_prepass.cpp`

Port from `Engine::DepthPrePass`. The pass becomes a render graph node that creates a transient depth texture and writes to it.

- [ ] **Step 1: Define the depth prepass graph node function**

```cpp
// depth_prepass.h
#pragma once

#include "helios-renderer/src/graph/render_graph.h"
#include "helios-renderer/src/frame_packet.h"
#include "helios-renderer/src/pipeline_cache.h"

namespace helios {

struct DepthPrepassData {
    TextureHandle depth_output;
};

// Adds the depth prepass to the render graph.
// Returns the handle to the depth texture produced by this pass.
TextureHandle add_depth_prepass(
    RenderGraph& graph,
    const FramePacket& packet,
    PipelineCache& cache);

} // namespace helios
```

- [ ] **Step 2: Implement the depth prepass**

The setup lambda creates a transient depth texture. The execute lambda binds the depth-only pipeline, uploads camera UBO, and draws all meshes depth-only.

```cpp
// depth_prepass.cpp
#include "depth_prepass.h"
#include "gpu_data.h"

namespace helios {

TextureHandle add_depth_prepass(
    RenderGraph& graph,
    const FramePacket& packet,
    PipelineCache& cache)
{
    struct PassData {
        TextureHandle depth;
    };

    TextureHandle result;

    graph.add_pass<PassData>(
        "DepthPrepass",
        [&](PassData& data, RenderGraphBuilder& builder) {
            // Create transient depth texture at viewport resolution
            data.depth = builder.create(TextureDesc{
                .width = packet.viewport_width,
                .height = packet.viewport_height,
                .format = TextureFormat::Depth32F,
                .usage = TextureUsage::DepthAttachment | TextureUsage::Sampled,
                .debug_name = "DepthPrepassTexture",
            });
            data.depth = builder.write(data.depth);
            result = data.depth;
        },
        [&packet, &cache](const PassData& data, RenderContext& ctx) {
            auto& cmd = ctx.cmd();
            auto& depth_tex = ctx.resolve(data.depth);
            uint32_t w = packet.viewport_width;
            uint32_t h = packet.viewport_height;

            // Upload camera matrices
            CameraUBOData cam_data{
                .view = packet.camera.view,
                .projection = packet.camera.projection,
            };
            // The pipeline cache gives us the pre-created depth prepass pipeline
            auto* pipeline = cache.get_graphics_pipeline("depth_prepass");
            if (!pipeline) return;

            cmd.begin_render_pass(RenderPassDesc{
                .depth_attachment = {
                    .texture = &depth_tex,
                    .load_op = LoadOp::Clear,
                    .store_op = StoreOp::Store,
                    .clear_depth = 1.0f,
                },
                .width = w, .height = h,
            });
            cmd.bind_pipeline(*pipeline);
            cmd.set_viewport(0, 0, static_cast<float>(w), static_cast<float>(h));
            cmd.set_scissor(0, 0, w, h);

            // Bind camera UBO at set 0, binding 0
            // (allocated as a transient buffer by the graph or from a ring buffer)
            cmd.push_constants(ShaderStage::Vertex, 0, sizeof(CameraUBOData), &cam_data);

            // Draw all mesh submeshes depth-only
            for (const auto& draw : packet.mesh_draws) {
                // AssetServer resolves mesh handle to GPU buffers
                // Each submesh: bind VBO/IBO, push model transform, draw indexed
                PushConstantData pc{ .transform = draw.transform };
                cmd.push_constants(ShaderStage::Vertex, 0, sizeof(pc), &pc);
                // cmd.bind_vertex_buffer(...)
                // cmd.bind_index_buffer(...)
                // cmd.draw_indexed(submesh.index_count)
            }

            cmd.end_render_pass();
        }
    );

    return result;
}

} // namespace helios
```

**Note:** The actual mesh data resolution (AssetHandle to GPU buffers) depends on the AssetServer resource from Plan 5. The draw loop shows the structure; asset resolution will be wired in when that plan is complete.

- [ ] **Step 3: Verify the pass registers in the render graph with correct resource declarations**

---

## Task 8: Shadow Pass Graph Node

**Files:**
- Create: `helios-renderer/src/forward_plus/shadow_pass.h`
- Create: `helios-renderer/src/forward_plus/shadow_pass.cpp`

Port from `Engine::ShadowPass`. Produces a cascade shadow map (depth-only 2D array texture). Includes cascade matrix computation logic from `Renderer::ComputeCascadeLightMatrices`.

- [ ] **Step 1: Define cascade matrix computation**

```cpp
// shadow_pass.h
#pragma once

#include "helios-renderer/src/graph/render_graph.h"
#include "helios-renderer/src/frame_packet.h"
#include "helios-renderer/src/pipeline_cache.h"
#include "forward_plus_plugin.h"

namespace helios {

struct ShadowPassOutput {
    TextureHandle shadow_map;
    std::vector<glm::mat4> cascade_matrices;
};

// Compute cascade split distances (geometric splits from camera planes)
std::vector<float> compute_cascade_splits(
    float near_plane, float far_plane, uint32_t cascade_count);

// Compute light-space matrix for a single cascade
glm::mat4 compute_light_space_matrix(
    const CameraData& camera,
    float split_near, float split_far,
    const glm::vec3& light_dir);

// Compute all cascade matrices
std::vector<glm::mat4> compute_cascade_matrices(
    const CameraData& camera,
    const glm::vec3& light_dir,
    const std::vector<float>& splits);

// Add shadow pass to render graph
ShadowPassOutput add_shadow_pass(
    RenderGraph& graph,
    const FramePacket& packet,
    const ForwardPlusConfig& config,
    PipelineCache& cache);

} // namespace helios
```

- [ ] **Step 2: Implement cascade matrix computation**

Port from `Renderer::ComputeCascadeLightMatrices` and `Renderer::ComputeLightSpaceMatrix`.

```cpp
// shadow_pass.cpp (cascade matrix portion)
#include "shadow_pass.h"
#include "gpu_data.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <limits>

namespace helios {

std::vector<float> compute_cascade_splits(
    float near_plane, float far_plane, uint32_t cascade_count)
{
    std::vector<float> splits;
    splits.reserve(cascade_count - 1);
    // Geometric split scheme (same distribution as current engine)
    // Current: far/50, far/25, far/10, far/2
    float ratios[] = { 50.0f, 25.0f, 10.0f, 2.0f };
    for (uint32_t i = 0; i < cascade_count - 1 && i < 4; i++) {
        splits.push_back(far_plane / ratios[i]);
    }
    return splits;
}

glm::mat4 compute_light_space_matrix(
    const CameraData& camera,
    float split_near, float split_far,
    const glm::vec3& light_dir)
{
    glm::mat4 proj = glm::perspective(
        glm::radians(camera.fov_y), camera.aspect_ratio,
        split_near, split_far);
    glm::mat4 inv = glm::inverse(proj * camera.view);

    // Frustum corners in world space
    std::array<glm::vec4, 8> corners;
    int idx = 0;
    for (int x = 0; x < 2; x++)
        for (int y = 0; y < 2; y++)
            for (int z = 0; z < 2; z++)
                corners[idx++] = inv * glm::vec4(
                    2.0f * x - 1.0f,
                    2.0f * y - 1.0f,
                    2.0f * z - 1.0f, 1.0f);

    // Perspective divide
    for (auto& c : corners)
        c /= c.w;

    // Frustum center
    glm::vec3 center(0.0f);
    for (const auto& c : corners)
        center += glm::vec3(c);
    center /= 8.0f;

    glm::mat4 light_view = glm::lookAt(
        center - glm::normalize(light_dir), center,
        glm::vec3(0.0f, 1.0f, 0.0f));

    // AABB in light space
    float min_x = std::numeric_limits<float>::max();
    float max_x = std::numeric_limits<float>::lowest();
    float min_y = std::numeric_limits<float>::max();
    float max_y = std::numeric_limits<float>::lowest();
    float min_z = std::numeric_limits<float>::max();
    float max_z = std::numeric_limits<float>::lowest();

    for (const auto& c : corners) {
        glm::vec4 lc = light_view * c;
        min_x = std::min(min_x, lc.x); max_x = std::max(max_x, lc.x);
        min_y = std::min(min_y, lc.y); max_y = std::max(max_y, lc.y);
        min_z = std::min(min_z, lc.z); max_z = std::max(max_z, lc.z);
    }

    // Extend Z range to capture shadow casters behind frustum
    constexpr float z_mult = 10.0f;
    min_z = (min_z < 0) ? min_z * z_mult : min_z / z_mult;
    max_z = (max_z < 0) ? max_z / z_mult : max_z * z_mult;

    return glm::ortho(min_x, max_x, min_y, max_y, min_z, max_z) * light_view;
}

std::vector<glm::mat4> compute_cascade_matrices(
    const CameraData& camera,
    const glm::vec3& light_dir,
    const std::vector<float>& splits)
{
    std::vector<glm::mat4> matrices;
    float last_split = camera.near_plane;
    for (size_t i = 0; i <= splits.size(); i++) {
        float split_end = (i < splits.size()) ? splits[i] : camera.far_plane;
        matrices.push_back(
            compute_light_space_matrix(camera, last_split, split_end, light_dir));
        last_split = split_end;
    }
    return matrices;
}
```

- [ ] **Step 3: Implement the shadow pass render graph node**

```cpp
// shadow_pass.cpp (graph node portion)

ShadowPassOutput add_shadow_pass(
    RenderGraph& graph,
    const FramePacket& packet,
    const ForwardPlusConfig& config,
    PipelineCache& cache)
{
    ShadowPassOutput output;

    // If no shadow-casting directional lights, return empty
    bool has_shadow_caster = false;
    glm::vec3 light_dir{0, -1, 0};
    for (const auto& dl : packet.dir_lights) {
        if (dl.cast_shadows) {
            has_shadow_caster = true;
            light_dir = dl.direction;
            break;
        }
    }

    if (!has_shadow_caster) {
        output.shadow_map = {}; // null handle
        return output;
    }

    // Compute cascade matrices on CPU
    auto splits = config.cascade_splits.empty()
        ? compute_cascade_splits(packet.camera.near_plane,
                                 packet.camera.far_plane,
                                 config.shadow_cascades)
        : config.cascade_splits;
    output.cascade_matrices = compute_cascade_matrices(
        packet.camera, light_dir, splits);

    struct PassData {
        TextureHandle shadow_map;
    };

    graph.add_pass<PassData>(
        "ShadowPass",
        [&](PassData& data, RenderGraphBuilder& builder) {
            uint32_t res = config.shadow_resolution;
            uint32_t layers = static_cast<uint32_t>(
                output.cascade_matrices.size());
            data.shadow_map = builder.create(TextureDesc{
                .width = res, .height = res,
                .format = TextureFormat::Depth32F,
                .type = TextureType::Texture2DArray,
                .array_layers = layers,
                .usage = TextureUsage::DepthAttachment | TextureUsage::Sampled,
                .debug_name = "ShadowMap",
            });
            data.shadow_map = builder.write(data.shadow_map);
            output.shadow_map = data.shadow_map;
        },
        [&packet, &cache, matrices = output.cascade_matrices,
         res = config.shadow_resolution]
        (const PassData& data, RenderContext& ctx) {
            auto& cmd = ctx.cmd();
            auto& shadow_tex = ctx.resolve(data.shadow_map);

            auto* pipeline = cache.get_graphics_pipeline("shadow_pass");
            if (!pipeline) return;

            // Upload light matrices UBO
            // (Geometry shader uses these to render to all cascade layers)

            cmd.begin_render_pass(RenderPassDesc{
                .depth_attachment = {
                    .texture = &shadow_tex,
                    .load_op = LoadOp::Clear,
                    .store_op = StoreOp::Store,
                    .clear_depth = 1.0f,
                },
                .width = res, .height = res,
            });
            cmd.bind_pipeline(*pipeline);
            cmd.set_viewport(0, 0, static_cast<float>(res),
                             static_cast<float>(res));
            cmd.set_scissor(0, 0, res, res);

            // Bind light matrices UBO (set 0, binding 0)
            // Bind descriptor set with cascade matrices

            for (const auto& draw : packet.mesh_draws) {
                PushConstantData pc{ .transform = draw.transform };
                cmd.push_constants(ShaderStage::Vertex, 0, sizeof(pc), &pc);
                // cmd.bind_vertex_buffer(...)
                // cmd.bind_index_buffer(...)
                // cmd.draw_indexed(...)
            }

            cmd.end_render_pass();
        }
    );

    return output;
}

} // namespace helios
```

- [ ] **Step 4: Verify cascade matrix computation produces same results as `Renderer::ComputeCascadeLightMatrices`**

---

## Task 9: Light Culling Compute Pass Graph Node

**Files:**
- Create: `helios-renderer/src/forward_plus/light_culling.h`
- Create: `helios-renderer/src/forward_plus/light_culling.cpp`

Port from `Engine::LightCullingPass`. This is a compute dispatch that reads the depth buffer and point light data, then writes per-tile visible light indices.

- [ ] **Step 1: Define the light culling node**

```cpp
// light_culling.h
#pragma once

#include "helios-renderer/src/graph/render_graph.h"
#include "helios-renderer/src/frame_packet.h"
#include "helios-renderer/src/pipeline_cache.h"
#include "forward_plus_plugin.h"

namespace helios {

struct LightCullingOutput {
    BufferHandle light_ssbo;
    BufferHandle dir_light_ssbo;
    BufferHandle visible_indices_ssbo;
};

// Adds the tile-based light culling compute pass to the render graph.
// Reads the depth texture from the depth prepass.
// Writes per-tile visible light index lists.
LightCullingOutput add_light_culling_pass(
    RenderGraph& graph,
    TextureHandle depth_texture,
    const FramePacket& packet,
    const ForwardPlusConfig& config,
    PipelineCache& cache);

} // namespace helios
```

- [ ] **Step 2: Implement the light culling compute node**

```cpp
// light_culling.cpp
#include "light_culling.h"
#include "gpu_data.h"

namespace helios {

LightCullingOutput add_light_culling_pass(
    RenderGraph& graph,
    TextureHandle depth_texture,
    const FramePacket& packet,
    const ForwardPlusConfig& config,
    PipelineCache& cache)
{
    LightCullingOutput output;

    struct PassData {
        TextureHandle depth_input;
        BufferHandle light_ssbo;
        BufferHandle dir_light_ssbo;
        BufferHandle visible_indices;
        BufferHandle params_ubo;
    };

    uint32_t tile_size = config.tile_size;
    uint32_t tiles_x = (packet.viewport_width + tile_size - 1) / tile_size;
    uint32_t tiles_y = (packet.viewport_height + tile_size - 1) / tile_size;
    uint32_t num_tiles = tiles_x * tiles_y;

    graph.add_pass<PassData>(
        "LightCulling",
        [&](PassData& data, RenderGraphBuilder& builder) {
            // Read depth texture from prepass
            data.depth_input = builder.read(depth_texture);

            // Create light SSBO (CPU-uploaded per frame)
            data.light_ssbo = builder.create(BufferDesc{
                .size = sizeof(PointLightGPU) * MAX_POINT_LIGHTS,
                .usage = BufferUsage::Storage | BufferUsage::Transfer,
                .debug_name = "LightSSBO",
            });
            data.light_ssbo = builder.write(data.light_ssbo);
            output.light_ssbo = data.light_ssbo;

            // Directional light SSBO
            data.dir_light_ssbo = builder.create(BufferDesc{
                .size = sizeof(DirLightGPU) * MAX_DIR_LIGHTS,
                .usage = BufferUsage::Storage | BufferUsage::Transfer,
                .debug_name = "DirLightSSBO",
            });
            data.dir_light_ssbo = builder.write(data.dir_light_ssbo);
            output.dir_light_ssbo = data.dir_light_ssbo;

            // Visible indices SSBO
            uint32_t vis_size = num_tiles * sizeof(VisibleIndex)
                                * MAX_VISIBLE_LIGHTS_PER_TILE;
            data.visible_indices = builder.create(BufferDesc{
                .size = vis_size,
                .usage = BufferUsage::Storage | BufferUsage::Transfer,
                .debug_name = "VisibleIndicesSSBO",
            });
            data.visible_indices = builder.write(data.visible_indices);
            output.visible_indices_ssbo = data.visible_indices;

            // Params UBO
            data.params_ubo = builder.create(BufferDesc{
                .size = sizeof(LightCullingParams),
                .usage = BufferUsage::Uniform,
                .debug_name = "LightCullingParamsUBO",
            });
            data.params_ubo = builder.write(data.params_ubo);
        },
        [&packet, tiles_x, tiles_y, &cache]
        (const PassData& data, RenderContext& ctx) {
            auto& cmd = ctx.cmd();

            // Upload point lights to SSBO
            auto& light_buf = ctx.resolve(data.light_ssbo);
            std::vector<PointLightGPU> gpu_lights(packet.point_lights.size());
            for (size_t i = 0; i < packet.point_lights.size(); i++) {
                const auto& pl = packet.point_lights[i];
                gpu_lights[i] = PointLightGPU{
                    .constant_attenuation = pl.constant_attenuation,
                    .linear_attenuation = pl.linear_attenuation,
                    .quadratic_attenuation = pl.quadratic_attenuation,
                    .intensity = pl.intensity,
                    .color = glm::vec4(pl.color, 1.0f),
                    .position = glm::vec4(pl.position, 1.0f),
                };
            }
            if (!gpu_lights.empty())
                light_buf.set_data(gpu_lights.data(),
                    gpu_lights.size() * sizeof(PointLightGPU));

            // Upload directional lights
            auto& dir_buf = ctx.resolve(data.dir_light_ssbo);
            std::vector<DirLightGPU> gpu_dirs(packet.dir_lights.size());
            for (size_t i = 0; i < packet.dir_lights.size(); i++) {
                const auto& dl = packet.dir_lights[i];
                gpu_dirs[i] = DirLightGPU{
                    ._padding = glm::vec3(1.0f),
                    .intensity = dl.intensity,
                    .color = glm::vec4(dl.color, 1.0f),
                    .direction = glm::vec4(dl.direction, 1.0f),
                };
            }
            if (!gpu_dirs.empty())
                dir_buf.set_data(gpu_dirs.data(),
                    gpu_dirs.size() * sizeof(DirLightGPU));

            // Initialize visible indices with -1 sentinel
            auto& vis_buf = ctx.resolve(data.visible_indices);
            uint32_t vis_size = vis_buf.size();
            std::vector<int> sentinel(vis_size / sizeof(int), -1);
            vis_buf.set_data(sentinel.data(), vis_size);

            // Upload params UBO
            auto& params_buf = ctx.resolve(data.params_ubo);
            LightCullingParams params{
                .view = packet.camera.view,
                .projection = packet.camera.projection,
                .screen_size = glm::ivec2(
                    packet.viewport_width, packet.viewport_height),
                .light_count = static_cast<int>(packet.point_lights.size()),
            };
            params_buf.set_data(&params, sizeof(params));

            // Bind compute pipeline and dispatch
            auto* pipeline = cache.get_compute_pipeline("light_culling");
            if (!pipeline) return;

            cmd.bind_pipeline(*pipeline);
            // Bind descriptor set: params UBO, light SSBO, visible indices, depth
            cmd.dispatch(tiles_x, tiles_y, 1);
        }
    );

    return output;
}

} // namespace helios
```

- [ ] **Step 3: Verify tile calculation matches existing code: `(width + 15) / 16` tiles**

---

## Task 10: Forward Pass Graph Node

**Files:**
- Create: `helios-renderer/src/forward_plus/forward_pass.h`
- Create: `helios-renderer/src/forward_plus/forward_pass.cpp`

Port from `Engine::ForwardPass`. This is the main PBR rendering pass that reads the depth prepass, shadow map, light culling results, and IBL textures, then renders all meshes with full lighting.

- [ ] **Step 1: Define the forward pass node**

```cpp
// forward_pass.h
#pragma once

#include "helios-renderer/src/graph/render_graph.h"
#include "helios-renderer/src/frame_packet.h"
#include "helios-renderer/src/pipeline_cache.h"
#include "forward_plus_plugin.h"
#include "light_culling.h"
#include "shadow_pass.h"

namespace helios {

// Adds the forward PBR pass to the render graph.
// Reads: depth texture, shadow map, light culling buffers, IBL textures.
// Writes: HDR color texture (RGBA16F).
// Returns: handle to the HDR color output.
TextureHandle add_forward_pass(
    RenderGraph& graph,
    TextureHandle depth_texture,
    const ShadowPassOutput& shadows,
    const LightCullingOutput& light_cull,
    const FramePacket& packet,
    const ForwardPlusConfig& config,
    PipelineCache& cache);

} // namespace helios
```

- [ ] **Step 2: Implement the forward pass graph node**

```cpp
// forward_pass.cpp
#include "forward_pass.h"
#include "gpu_data.h"

namespace helios {

TextureHandle add_forward_pass(
    RenderGraph& graph,
    TextureHandle depth_texture,
    const ShadowPassOutput& shadows,
    const LightCullingOutput& light_cull,
    const FramePacket& packet,
    const ForwardPlusConfig& config,
    PipelineCache& cache)
{
    struct PassData {
        TextureHandle hdr_color;
        TextureHandle depth;
        TextureHandle shadow_map;
        BufferHandle light_ssbo;
        BufferHandle dir_light_ssbo;
        BufferHandle visible_indices;
    };

    TextureHandle result;
    uint32_t w = packet.viewport_width;
    uint32_t h = packet.viewport_height;
    uint32_t tiles_x = (w + TILE_SIZE - 1) / TILE_SIZE;

    graph.add_pass<PassData>(
        "ForwardPass",
        [&](PassData& data, RenderGraphBuilder& builder) {
            // Create HDR color output
            data.hdr_color = builder.create(TextureDesc{
                .width = w, .height = h,
                .format = config.hdr_format,
                .usage = TextureUsage::ColorAttachment | TextureUsage::Sampled,
                .debug_name = "ForwardHDRColor",
            });
            data.hdr_color = builder.write(data.hdr_color);
            result = data.hdr_color;

            // Create depth attachment for this pass
            data.depth = builder.create(TextureDesc{
                .width = w, .height = h,
                .format = config.depth_format,
                .usage = TextureUsage::DepthAttachment,
                .debug_name = "ForwardDepth",
            });
            data.depth = builder.write(data.depth);

            // Read light culling outputs
            data.light_ssbo = builder.read(light_cull.light_ssbo);
            data.dir_light_ssbo = builder.read(light_cull.dir_light_ssbo);
            data.visible_indices = builder.read(light_cull.visible_indices_ssbo);

            // Read shadow map (if valid)
            if (shadows.shadow_map.index != 0)
                data.shadow_map = builder.read(shadows.shadow_map);
        },
        [&packet, &cache, tiles_x, w, h,
         &shadows]
        (const PassData& data, RenderContext& ctx) {
            auto& cmd = ctx.cmd();
            auto& color_tex = ctx.resolve(data.hdr_color);
            auto& depth_tex = ctx.resolve(data.depth);

            auto* pipeline = cache.get_graphics_pipeline("forward_pass");
            if (!pipeline) return;

            // Upload GlobalUBO
            GlobalUBOData global{
                .camera_view = packet.camera.view,
                .camera_projection = packet.camera.projection,
                .camera_pos = packet.camera.position,
                .camera_far_plane = packet.camera.far_plane,
                .num_directional_lights = static_cast<int>(
                    packet.dir_lights.size()),
                .num_tiles_x = static_cast<int>(tiles_x),
                .environment_brightness = packet.skybox.brightness,
            };

            cmd.begin_render_pass(RenderPassDesc{
                .color_attachments = {{
                    .texture = &color_tex,
                    .load_op = LoadOp::Clear,
                    .store_op = StoreOp::Store,
                    .clear_color = {0.0f, 0.0f, 0.0f, 1.0f},
                }},
                .depth_attachment = {
                    .texture = &depth_tex,
                    .load_op = LoadOp::Clear,
                    .store_op = StoreOp::Store,
                    .clear_depth = 1.0f,
                },
                .width = w, .height = h,
            });
            cmd.bind_pipeline(*pipeline);
            cmd.set_viewport(0, 0, static_cast<float>(w),
                             static_cast<float>(h));
            cmd.set_scissor(0, 0, w, h);

            // Bind global descriptor set (set 0):
            //   binding 0: GlobalUBO
            //   binding 1: LightSSBO
            //   binding 2: DirLightSSBO
            //   binding 3: VisibleIndicesSSBO
            //   binding 4: LightMatricesUBO

            // Per-mesh: bind material descriptor set (set 1), push transform
            for (const auto& draw : packet.mesh_draws) {
                PushConstantData pc{ .transform = draw.transform };
                cmd.push_constants(ShaderStage::Vertex, 0, sizeof(pc), &pc);

                // Bind material descriptor set at set 1
                // Contains: MaterialUBO, normal/roughness/metalness/albedo/
                //   ao/emissive/specular textures, irradiance, prefilter,
                //   BRDF LUT, shadow map
                // (Material resolution via AssetServer)

                // cmd.bind_vertex_buffer(...)
                // cmd.bind_index_buffer(...)
                // cmd.draw_indexed(...)
            }

            // Note: render pass is NOT ended here if skybox follows
            // in the same pass, or it IS ended if skybox is separate.
            // In the graph model, skybox is a separate pass that reads
            // the same color attachment.
            cmd.end_render_pass();
        }
    );

    return result;
}

} // namespace helios
```

- [ ] **Step 3: Verify descriptor set layouts match existing shader bindings (set 0 global, set 1 material)**

---

## Task 11: Skybox Pass Graph Node

**Files:**
- Create: `helios-renderer/src/forward_plus/skybox_pass.h`
- Create: `helios-renderer/src/forward_plus/skybox_pass.cpp`

Port from `Engine::SkyboxRenderer`. Renders the environment cubemap behind all geometry. In the new graph, this reads and writes the same HDR color texture as the forward pass (depth test enabled, depth write disabled, drawn as last depth = 1.0 via `gl_Position = clipPos.xyww`).

- [ ] **Step 1: Define the skybox pass node**

```cpp
// skybox_pass.h
#pragma once

#include "helios-renderer/src/graph/render_graph.h"
#include "helios-renderer/src/frame_packet.h"
#include "helios-renderer/src/pipeline_cache.h"

namespace helios {

// Adds the skybox pass to the render graph.
// Reads/writes the HDR color texture (renders behind all geometry).
// Returns the same HDR color handle (modified in place).
TextureHandle add_skybox_pass(
    RenderGraph& graph,
    TextureHandle hdr_color,
    const FramePacket& packet,
    PipelineCache& cache);

} // namespace helios
```

- [ ] **Step 2: Implement the skybox pass**

```cpp
// skybox_pass.cpp
#include "skybox_pass.h"
#include "gpu_data.h"

namespace helios {

// Unit cube vertex/index data for skybox rendering
// (Positions only, 36 indices for 12 triangles)
namespace {
    constexpr float CUBE_VERTICES[] = {
        -1.0f,  1.0f, -1.0f,  -1.0f, -1.0f, -1.0f,   1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,   1.0f,  1.0f, -1.0f,  -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,  -1.0f, -1.0f, -1.0f,  -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,  -1.0f,  1.0f,  1.0f,  -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f, -1.0f,   1.0f, -1.0f,  1.0f,   1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,   1.0f,  1.0f, -1.0f,   1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,  -1.0f,  1.0f,  1.0f,   1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,   1.0f, -1.0f,  1.0f,  -1.0f, -1.0f,  1.0f,
        -1.0f,  1.0f, -1.0f,   1.0f,  1.0f, -1.0f,   1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,  -1.0f,  1.0f,  1.0f,  -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,  -1.0f, -1.0f,  1.0f,   1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,  -1.0f, -1.0f,  1.0f,   1.0f, -1.0f,  1.0f,
    };
    constexpr uint32_t CUBE_VERTEX_COUNT = 36;
} // anon namespace

TextureHandle add_skybox_pass(
    RenderGraph& graph,
    TextureHandle hdr_color,
    const FramePacket& packet,
    PipelineCache& cache)
{
    struct PassData {
        TextureHandle color;
    };

    TextureHandle result;

    graph.add_pass<PassData>(
        "SkyboxPass",
        [&](PassData& data, RenderGraphBuilder& builder) {
            // Read-modify-write the HDR color texture
            data.color = builder.write(hdr_color);
            result = data.color;
        },
        [&packet, &cache](const PassData& data, RenderContext& ctx) {
            auto& cmd = ctx.cmd();

            auto* pipeline = cache.get_graphics_pipeline("skybox");
            if (!pipeline) return;

            // Only render if skybox environment map is available
            if (!packet.skybox.environment_map) return;

            // Upload SkyboxUBO
            SkyboxUBOData ubo{
                .camera_view = packet.camera.view,
                .camera_projection = packet.camera.projection,
                .brightness = packet.skybox.brightness,
            };

            // Note: render pass is either shared with forward pass
            // (the graph may merge them) or standalone.
            // The pipeline uses depth test (<=) but no depth write,
            // and the vertex shader outputs clipPos.xyww so skybox
            // only renders where nothing else drew.

            cmd.bind_pipeline(*pipeline);
            // Bind descriptor set: SkyboxUBO (binding 0), cubemap (binding 1)
            // Draw unit cube (36 vertices, no index buffer)
            cmd.draw(CUBE_VERTEX_COUNT);
        }
    );

    return result;
}

} // namespace helios
```

- [ ] **Step 3: Verify skybox pipeline state: depth test enabled (LessEqual), depth write disabled, cull mode None**

---

## Task 12: Tonemap Pass Graph Node

**Files:**
- Create: `helios-renderer/src/forward_plus/tonemap_pass.h`
- Create: `helios-renderer/src/forward_plus/tonemap_pass.cpp`

Port from the tonemap compute dispatch in `Renderer::BeginDrawing`. This is a compute pass that reads the HDR color texture and writes an LDR (RGBA8) output.

- [ ] **Step 1: Define the tonemap pass node**

```cpp
// tonemap_pass.h
#pragma once

#include "helios-renderer/src/graph/render_graph.h"
#include "helios-renderer/src/frame_packet.h"
#include "helios-renderer/src/pipeline_cache.h"
#include "forward_plus_plugin.h"

namespace helios {

// Adds the tonemap compute pass to the render graph.
// Reads the HDR color texture, writes an LDR (RGBA8) output.
// Uses exposure value from ForwardPlusConfig.
TextureHandle add_tonemap_pass(
    RenderGraph& graph,
    TextureHandle hdr_input,
    const FramePacket& packet,
    const ForwardPlusConfig& config,
    PipelineCache& cache);

} // namespace helios
```

- [ ] **Step 2: Implement the tonemap compute pass**

```cpp
// tonemap_pass.cpp
#include "tonemap_pass.h"
#include "gpu_data.h"

namespace helios {

TextureHandle add_tonemap_pass(
    RenderGraph& graph,
    TextureHandle hdr_input,
    const FramePacket& packet,
    const ForwardPlusConfig& config,
    PipelineCache& cache)
{
    struct PassData {
        TextureHandle hdr;
        TextureHandle ldr;
    };

    TextureHandle result;
    uint32_t w = packet.viewport_width;
    uint32_t h = packet.viewport_height;

    graph.add_pass<PassData>(
        "TonemapPass",
        [&](PassData& data, RenderGraphBuilder& builder) {
            data.hdr = builder.read(hdr_input);
            data.ldr = builder.create(TextureDesc{
                .width = w, .height = h,
                .format = TextureFormat::RGBA8,
                .usage = TextureUsage::Sampled | TextureUsage::Storage,
                .debug_name = "TonemapLDR",
            });
            data.ldr = builder.write(data.ldr);
            result = data.ldr;
        },
        [w, h, exposure = config.exposure, &cache]
        (const PassData& data, RenderContext& ctx) {
            auto& cmd = ctx.cmd();

            auto* pipeline = cache.get_compute_pipeline("tonemap_compute");
            if (!pipeline) return;

            // Bind compute pipeline
            cmd.bind_pipeline(*pipeline);

            // Bind descriptor set:
            //   binding 0: HDR input (CombinedImageSampler)
            //   binding 1: LDR output (StorageImage)

            // Push exposure constant
            TonemapPushConstants pc{ .exposure = exposure };
            cmd.push_constants(ShaderStage::Compute, 0, sizeof(pc), &pc);

            uint32_t groups_x = (w + 15) / 16;
            uint32_t groups_y = (h + 15) / 16;
            cmd.dispatch(groups_x, groups_y, 1);
        }
    );

    return result;
}

} // namespace helios
```

- [ ] **Step 3: Verify dispatch group sizes match tonemap.comp workgroup size (16x16)**

---

## Task 13: build_forward_plus_graph System

**Files:**
- Modify: `helios-renderer/src/forward_plus/forward_plus_plugin.cpp`

This is the second system registered by the plugin. It wires all the passes together.

- [ ] **Step 1: Implement the graph builder system**

```cpp
// In forward_plus_plugin.cpp

void build_forward_plus_graph(
    Res<FramePacket> packet,
    Res<ForwardPlusConfig> config,
    ResMut<RenderGraph> graph,
    ResMut<PipelineCache> cache
) {
    // 1. Depth prepass
    auto depth = add_depth_prepass(*graph, *packet, *cache);

    // 2. Shadow pass (cascade shadow maps)
    auto shadows = add_shadow_pass(*graph, *packet, *config, *cache);

    // 3. Light culling (tile-based compute)
    auto light_cull = add_light_culling_pass(
        *graph, depth, *packet, *config, *cache);

    // 4. Forward PBR pass
    auto hdr = add_forward_pass(
        *graph, depth, shadows, light_cull,
        *packet, *config, *cache);

    // 5. Skybox pass (renders behind all geometry into HDR target)
    auto skybox = add_skybox_pass(*graph, hdr, *packet, *cache);

    // 6. Tonemap pass (HDR -> LDR)
    auto ldr = add_tonemap_pass(
        *graph, skybox, *packet, *config, *cache);

    // Set final output - the render graph will cull any passes
    // that don't contribute to this output
    graph->set_output(ldr);
}
```

- [ ] **Step 2: Include all pass headers in forward_plus_plugin.cpp**

```cpp
#include "depth_prepass.h"
#include "shadow_pass.h"
#include "light_culling.h"
#include "forward_pass.h"
#include "skybox_pass.h"
#include "tonemap_pass.h"
```

- [ ] **Step 3: Verify the complete pass ordering: depth -> shadows -> light_cull -> forward -> skybox -> tonemap**

---

## Task 14: Equirect-to-Cube Compute

**Files:**
- Create: `helios-renderer/src/forward_plus/equirect_to_cube.h`
- Create: `helios-renderer/src/forward_plus/equirect_to_cube.cpp`

Port from `Renderer::ConvertEquirectToCube`. This is a one-shot compute dispatch (not a per-frame pass) that converts an equirectangular HDR image to a cubemap.

- [ ] **Step 1: Define the equirect-to-cube function**

```cpp
// equirect_to_cube.h
#pragma once

#include "helios-renderer/src/rhi/rhi.h"
#include "helios-renderer/src/pipeline_cache.h"

namespace helios {

// Converts an equirectangular 2D texture to a cubemap via compute dispatch.
// This is a one-shot operation (not a per-frame render graph node).
// Uses ImmediateSubmit on the device.
// The output cubemap must already be created with appropriate size and format.
void convert_equirect_to_cube(
    rhi::Device& device,
    PipelineCache& cache,
    const rhi::Texture& equirect_input,
    rhi::Texture& cubemap_output,
    uint32_t cube_size);

} // namespace helios
```

- [ ] **Step 2: Implement the equirect-to-cube dispatch**

```cpp
// equirect_to_cube.cpp
#include "equirect_to_cube.h"

namespace helios {

void convert_equirect_to_cube(
    rhi::Device& device,
    PipelineCache& cache,
    const rhi::Texture& equirect_input,
    rhi::Texture& cubemap_output,
    uint32_t cube_size)
{
    // Load compute shader
    auto* shader = cache.load_shader(
        "shaders/equirect_to_cube.comp.spv", ShaderStage::Compute);
    if (!shader) return;

    // Descriptor layout: binding 0 = sampler2D, binding 1 = imageCube
    // Create pipeline (cached by name)
    // Allocate descriptor set
    // Write equirect as sampler, cubemap as storage image
    // Dispatch: ceil(cube_size/16), ceil(cube_size/16), 6 (faces)

    // The RHI device provides immediate_submit for one-shot GPU work:
    device.immediate_submit([&](rhi::CommandBuffer& cmd) {
        // Transition cubemap to General for storage write
        // (handled automatically by RHI if using barriers API)

        auto& pipeline = cache.get_or_create_compute(
            "equirect_to_cube", ComputePipelineDesc{
                // ... shader, layouts, etc.
            });

        cmd.bind_pipeline(pipeline);
        // Bind descriptor set
        uint32_t groups = (cube_size + 15) / 16;
        cmd.dispatch(groups, groups, 6);

        // Transition cubemap to ShaderReadOnly
    });
}

} // namespace helios
```

- [ ] **Step 3: Verify shader dispatch matches equirect_to_cube.comp (16x16x1 workgroup, z=face)**

---

## Task 15: IBL Generation (Irradiance, Prefilter, BRDF LUT)

**Files:**
- Create: `helios-renderer/src/forward_plus/ibl_generation.h`
- Create: `helios-renderer/src/forward_plus/ibl_generation.cpp`

Port from `Renderer::GenerateIBL`. All three IBL stages (irradiance convolution, prefilter environment map, BRDF LUT) are compute dispatches.

- [ ] **Step 1: Define IBL generation interface**

```cpp
// ibl_generation.h
#pragma once

#include "helios-renderer/src/rhi/rhi.h"
#include "helios-renderer/src/pipeline_cache.h"

namespace helios {

struct IBLTextures {
    rhi::Texture irradiance_map;   // Small cubemap (e.g. 32x32)
    rhi::Texture prefilter_map;    // Cubemap with mip chain (e.g. 128x128)
    rhi::Texture brdf_lut;         // 2D texture (e.g. 512x512, RG16F)
};

// Generate all IBL textures from an environment cubemap.
// This is a one-shot operation triggered when a skybox is loaded.
// All three stages run as immediate compute dispatches.
IBLTextures generate_ibl(
    rhi::Device& device,
    PipelineCache& cache,
    const rhi::Texture& environment_cubemap,
    uint32_t irradiance_size = 32,
    uint32_t prefilter_size = 128,
    uint32_t brdf_size = 512);

// Individual stages (can be called separately if needed):

// Irradiance convolution: environment cubemap -> small diffuse irradiance cubemap
rhi::Texture generate_irradiance_map(
    rhi::Device& device,
    PipelineCache& cache,
    const rhi::Texture& environment_cubemap,
    uint32_t size);

// Prefilter environment map: environment cubemap -> mipmap chain of
// increasingly blurred specular reflections
rhi::Texture generate_prefilter_map(
    rhi::Device& device,
    PipelineCache& cache,
    const rhi::Texture& environment_cubemap,
    uint32_t size);

// BRDF integration LUT: no input texture, purely mathematical
rhi::Texture generate_brdf_lut(
    rhi::Device& device,
    PipelineCache& cache,
    uint32_t size);

} // namespace helios
```

- [ ] **Step 2: Implement irradiance convolution**

Port from `Renderer::GenerateIBL()` irradiance section. Creates a small cubemap, dispatches `irradiance_convolution.comp` with the environment cubemap as input.

```cpp
// ibl_generation.cpp (irradiance portion)

rhi::Texture generate_irradiance_map(
    rhi::Device& device,
    PipelineCache& cache,
    const rhi::Texture& environment_cubemap,
    uint32_t size)
{
    // Create output cubemap
    rhi::Texture output(device, TextureDesc{
        .width = size, .height = size,
        .format = TextureFormat::RGBA16F,
        .type = TextureType::TextureCube,
        .mip_levels = 1,
        .array_layers = 6,
        .usage = TextureUsage::Sampled | TextureUsage::Storage
                 | TextureUsage::Transfer,
        .clamp_to_edge = true,
        .debug_name = "IrradianceMap",
    });

    auto* shader = cache.load_shader(
        "shaders/irradiance_convolution.comp.spv", ShaderStage::Compute);
    if (!shader) return std::move(output);

    // Create pipeline, descriptor set, dispatch
    // Layout: binding 0 = samplerCube (environment), binding 1 = imageCube (output)
    device.immediate_submit([&](rhi::CommandBuffer& cmd) {
        // Transition output UNDEFINED -> GENERAL
        auto& pipeline = cache.get_or_create_compute("irradiance_conv", /*...*/);
        cmd.bind_pipeline(pipeline);
        // Bind descriptors
        uint32_t groups = (size + 15) / 16;
        cmd.dispatch(groups, groups, 6);
        // Transition output GENERAL -> SHADER_READ_ONLY
    });

    return output;
}
```

- [ ] **Step 3: Implement prefilter environment map**

Port from `Renderer::GenerateIBL()` prefilter section. This dispatches once per mip level with push constants for roughness and mip size. Uses per-mip image views for storage writes.

```cpp
// ibl_generation.cpp (prefilter portion)

rhi::Texture generate_prefilter_map(
    rhi::Device& device,
    PipelineCache& cache,
    const rhi::Texture& environment_cubemap,
    uint32_t size)
{
    uint32_t max_mip_levels = static_cast<uint32_t>(
        std::floor(std::log2(static_cast<double>(size)))) + 1;

    rhi::Texture output(device, TextureDesc{
        .width = size, .height = size,
        .format = TextureFormat::RGBA16F,
        .type = TextureType::TextureCube,
        .mip_levels = max_mip_levels,
        .array_layers = 6,
        .usage = TextureUsage::Sampled | TextureUsage::Storage
                 | TextureUsage::Transfer,
        .clamp_to_edge = true,
        .debug_name = "PrefilterMap",
    });

    auto* shader = cache.load_shader(
        "shaders/prefilter_envmap.comp.spv", ShaderStage::Compute);
    if (!shader) return std::move(output);

    // Create pipeline with push constants: { float roughness, uint mip_size }
    // Create per-mip descriptor sets with per-mip image views

    device.immediate_submit([&](rhi::CommandBuffer& cmd) {
        // Transition output UNDEFINED -> GENERAL
        auto& pipeline = cache.get_or_create_compute("prefilter_envmap", /*...*/);
        cmd.bind_pipeline(pipeline);

        for (uint32_t mip = 0; mip < max_mip_levels; mip++) {
            uint32_t mip_size = std::max(1u, size >> mip);
            float roughness = static_cast<float>(mip)
                              / static_cast<float>(max_mip_levels - 1);

            PrefilterPushConstants pc{
                .roughness = roughness,
                .mip_size = mip_size,
            };
            cmd.push_constants(ShaderStage::Compute, 0, sizeof(pc), &pc);
            // Bind per-mip descriptor set
            uint32_t groups = (mip_size + 15) / 16;
            cmd.dispatch(groups, groups, 6);

            // Memory barrier between mip dispatches
            if (mip < max_mip_levels - 1) {
                cmd.pipeline_barrier(/*compute write -> compute write*/);
            }
        }

        // Transition output GENERAL -> SHADER_READ_ONLY
    });

    return output;
}
```

- [ ] **Step 4: Implement BRDF LUT generation**

Port from `Renderer::GenerateIBL()` BRDF section. No input texture needed -- purely mathematical.

```cpp
// ibl_generation.cpp (BRDF portion)

rhi::Texture generate_brdf_lut(
    rhi::Device& device,
    PipelineCache& cache,
    uint32_t size)
{
    rhi::Texture output(device, TextureDesc{
        .width = size, .height = size,
        .format = TextureFormat::RG16F,
        .type = TextureType::Texture2D,
        .mip_levels = 1,
        .usage = TextureUsage::Sampled | TextureUsage::Storage
                 | TextureUsage::Transfer,
        .clamp_to_edge = true,
        .debug_name = "BRDF_LUT",
    });

    auto* shader = cache.load_shader(
        "shaders/brdf_lut.comp.spv", ShaderStage::Compute);
    if (!shader) return std::move(output);

    // Layout: binding 0 = image2D (output, rg16f)
    device.immediate_submit([&](rhi::CommandBuffer& cmd) {
        // Transition UNDEFINED -> GENERAL
        auto& pipeline = cache.get_or_create_compute("brdf_lut", /*...*/);
        cmd.bind_pipeline(pipeline);
        // Bind descriptor
        uint32_t groups = (size + 15) / 16;
        cmd.dispatch(groups, groups, 1);
        // Transition GENERAL -> SHADER_READ_ONLY
    });

    return output;
}
```

- [ ] **Step 5: Implement the combined generate_ibl function**

```cpp
IBLTextures generate_ibl(
    rhi::Device& device,
    PipelineCache& cache,
    const rhi::Texture& environment_cubemap,
    uint32_t irradiance_size,
    uint32_t prefilter_size,
    uint32_t brdf_size)
{
    return IBLTextures{
        .irradiance_map = generate_irradiance_map(
            device, cache, environment_cubemap, irradiance_size),
        .prefilter_map = generate_prefilter_map(
            device, cache, environment_cubemap, prefilter_size),
        .brdf_lut = generate_brdf_lut(device, cache, brdf_size),
    };
}
```

- [ ] **Step 6: Verify all dispatch dimensions match existing compute shader workgroup sizes**

---

## Task 16: Shader Compilation CMake Function and Shader Migration

**Files:**
- Create: `cmake/compile_shaders.cmake`
- Create: `shaders/depth_prepass.vert`
- Create: `shaders/depth_prepass.frag`
- Create: `shaders/dir_light_shadows.vert`
- Create: `shaders/dir_light_shadows.frag`
- Create: `shaders/dir_light_shadows.geom`
- Create: `shaders/light_culling.comp`
- Create: `shaders/default_static.vert`
- Create: `shaders/default_static.frag`
- Create: `shaders/skybox.vert`
- Create: `shaders/skybox.frag`
- Create: `shaders/tonemap.comp`
- Create: `shaders/equirect_to_cube.comp`
- Create: `shaders/irradiance_convolution.comp`
- Create: `shaders/prefilter_envmap.comp`
- Create: `shaders/brdf_lut.comp`

- [ ] **Step 1: Create the CMake shader compilation function**

```cmake
# cmake/compile_shaders.cmake

# compile_shaders(
#   SOURCE_DIR <path>        - directory containing .vert/.frag/.comp/.geom files
#   OUTPUT_DIR <path>        - directory for compiled .spv files
#   BACKEND    <vulkan|...>  - currently only vulkan (GLSL -> SPIR-V via glslc)
#   TARGET     <name>        - optional: add as dependency of this target
# )
function(compile_shaders)
    cmake_parse_arguments(SHADER "" "SOURCE_DIR;OUTPUT_DIR;BACKEND;TARGET" "" ${ARGN})

    if(NOT SHADER_SOURCE_DIR)
        message(FATAL_ERROR "compile_shaders: SOURCE_DIR is required")
    endif()
    if(NOT SHADER_OUTPUT_DIR)
        message(FATAL_ERROR "compile_shaders: OUTPUT_DIR is required")
    endif()
    if(NOT SHADER_BACKEND)
        set(SHADER_BACKEND "vulkan")
    endif()

    # Find glslc
    find_program(GLSLC_EXECUTABLE glslc REQUIRED)

    # Collect all shader source files
    file(GLOB SHADER_SOURCES
        "${SHADER_SOURCE_DIR}/*.vert"
        "${SHADER_SOURCE_DIR}/*.frag"
        "${SHADER_SOURCE_DIR}/*.comp"
        "${SHADER_SOURCE_DIR}/*.geom"
    )

    # Ensure output directory exists
    file(MAKE_DIRECTORY "${SHADER_OUTPUT_DIR}")

    set(SPIRV_OUTPUTS "")

    foreach(SHADER_SRC ${SHADER_SOURCES})
        get_filename_component(SHADER_NAME ${SHADER_SRC} NAME)
        set(SPIRV_OUTPUT "${SHADER_OUTPUT_DIR}/${SHADER_NAME}.spv")

        # Determine shader stage for glslc
        # glslc auto-detects from extension for .vert/.frag/.comp
        # .geom needs explicit -fshader-stage=geometry
        set(STAGE_FLAG "")
        if(SHADER_SRC MATCHES "\\.geom$")
            set(STAGE_FLAG "-fshader-stage=geometry")
        endif()

        add_custom_command(
            OUTPUT ${SPIRV_OUTPUT}
            COMMAND ${GLSLC_EXECUTABLE}
                    ${STAGE_FLAG}
                    -o ${SPIRV_OUTPUT}
                    ${SHADER_SRC}
            DEPENDS ${SHADER_SRC}
            COMMENT "Compiling shader: ${SHADER_NAME} -> ${SHADER_NAME}.spv"
            VERBATIM
        )

        list(APPEND SPIRV_OUTPUTS ${SPIRV_OUTPUT})
    endforeach()

    # Create a custom target for all shaders
    add_custom_target(compile_shaders ALL DEPENDS ${SPIRV_OUTPUTS})

    # If a parent target is specified, add dependency
    if(SHADER_TARGET)
        add_dependencies(${SHADER_TARGET} compile_shaders)
    endif()
endfunction()
```

- [ ] **Step 2: Port all shaders from `Editor/Resources/Shaders/` to `shaders/`**

Copy each shader source file to the new `shaders/` directory at project root, renaming for consistency:

| Old path | New path |
|----------|----------|
| `Editor/Resources/Shaders/depth_pre_pass.vert` | `shaders/depth_prepass.vert` |
| `Editor/Resources/Shaders/depth_pre_pass.frag` | `shaders/depth_prepass.frag` |
| `Editor/Resources/Shaders/dir_light_shadows.vert` | `shaders/dir_light_shadows.vert` |
| `Editor/Resources/Shaders/dir_light_shadows.frag` | `shaders/dir_light_shadows.frag` |
| `Editor/Resources/Shaders/dir_light_shadows.geo` | `shaders/dir_light_shadows.geom` |
| `Editor/Resources/Shaders/light_culling_shader.comp` | `shaders/light_culling.comp` |
| `Editor/Resources/Shaders/default_static_shader.vert` | `shaders/default_static.vert` |
| `Editor/Resources/Shaders/default_static_shader.frag` | `shaders/default_static.frag` |
| `Editor/Resources/Shaders/skybox.vert` | `shaders/skybox.vert` |
| `Editor/Resources/Shaders/skybox.frag` | `shaders/skybox.frag` |
| `Editor/Resources/Shaders/tonemap.comp` | `shaders/tonemap.comp` |
| `Editor/Resources/Shaders/equirect_to_cube.comp` | `shaders/equirect_to_cube.comp` |
| `Editor/Resources/Shaders/irradiance_convolution.comp` | `shaders/irradiance_convolution.comp` |
| `Editor/Resources/Shaders/prefilter_envmap.comp` | `shaders/prefilter_envmap.comp` |
| `Editor/Resources/Shaders/brdf_lut.comp` | `shaders/brdf_lut.comp` |

The shader sources are identical -- the only change is file location and naming convention. The GLSL code is not modified.

- [ ] **Step 3: Add compile_shaders() call to helios-renderer CMakeLists.txt**

```cmake
# In helios-renderer/CMakeLists.txt
include(${CMAKE_SOURCE_DIR}/cmake/compile_shaders.cmake)

compile_shaders(
    SOURCE_DIR ${CMAKE_SOURCE_DIR}/shaders
    OUTPUT_DIR ${CMAKE_BINARY_DIR}/shaders
    BACKEND vulkan
    TARGET helios-renderer
)
```

- [ ] **Step 4: Verify all shaders compile with glslc (run `cmake --build . --target compile_shaders`)**

---

## Task 17: Pipeline Initialization (Creating Pipelines at Startup)

**Files:**
- Modify: `helios-renderer/src/forward_plus/forward_plus_plugin.cpp`

The plugin's `build()` method must pre-create all pipelines so they are cached and ready when the first frame renders.

- [ ] **Step 1: Add pipeline initialization function**

```cpp
// In forward_plus_plugin.cpp

static void initialize_pipelines(rhi::Device& device, PipelineCache& cache) {
    // Depth prepass pipeline
    {
        auto* vert = cache.load_shader("shaders/depth_prepass.vert.spv",
                                        ShaderStage::Vertex);
        auto* frag = cache.load_shader("shaders/depth_prepass.frag.spv",
                                        ShaderStage::Fragment);
        if (vert && frag) {
            cache.get_or_create_graphics("depth_prepass", GraphicsPipelineDesc{
                .vertex_shader = vert,
                .fragment_shader = frag,
                .vertex_layout = get_mesh_vertex_layout(),
                .push_constant_size = sizeof(PushConstantData),
                .push_constant_stages = ShaderStage::Vertex,
                .state = {
                    .depth_write = true,
                    .depth_test = true,
                    .cull_mode = CullMode::Back,
                },
                .debug_name = "DepthPrepassPipeline",
            });
        }
    }

    // Shadow pass pipeline
    {
        auto* vert = cache.load_shader("shaders/dir_light_shadows.vert.spv",
                                        ShaderStage::Vertex);
        auto* frag = cache.load_shader("shaders/dir_light_shadows.frag.spv",
                                        ShaderStage::Fragment);
        auto* geom = cache.load_shader("shaders/dir_light_shadows.geom.spv",
                                        ShaderStage::Geometry);
        if (vert && frag) {
            cache.get_or_create_graphics("shadow_pass", GraphicsPipelineDesc{
                .vertex_shader = vert,
                .fragment_shader = frag,
                .geometry_shader = geom,
                .vertex_layout = get_mesh_vertex_layout(),
                .push_constant_size = sizeof(PushConstantData),
                .push_constant_stages = ShaderStage::Vertex,
                .state = {
                    .depth_write = true,
                    .depth_test = true,
                    .cull_mode = CullMode::Front, // front-face for shadow bias
                },
                .debug_name = "ShadowPassPipeline",
            });
        }
    }

    // Light culling compute pipeline
    {
        auto* comp = cache.load_shader("shaders/light_culling.comp.spv",
                                        ShaderStage::Compute);
        if (comp) {
            cache.get_or_create_compute("light_culling", ComputePipelineDesc{
                .compute_shader = comp,
                .debug_name = "LightCullingPipeline",
            });
        }
    }

    // Forward pass pipeline
    {
        auto* vert = cache.load_shader("shaders/default_static.vert.spv",
                                        ShaderStage::Vertex);
        auto* frag = cache.load_shader("shaders/default_static.frag.spv",
                                        ShaderStage::Fragment);
        if (vert && frag) {
            cache.get_or_create_graphics("forward_pass", GraphicsPipelineDesc{
                .vertex_shader = vert,
                .fragment_shader = frag,
                .vertex_layout = get_mesh_vertex_layout(),
                .push_constant_size = sizeof(PushConstantData),
                .push_constant_stages = ShaderStage::Vertex,
                .state = {
                    .depth_write = true,
                    .depth_test = true,
                    .cull_mode = CullMode::Back,
                },
                .debug_name = "ForwardPassPipeline",
            });
        }
    }

    // Skybox pipeline
    {
        auto* vert = cache.load_shader("shaders/skybox.vert.spv",
                                        ShaderStage::Vertex);
        auto* frag = cache.load_shader("shaders/skybox.frag.spv",
                                        ShaderStage::Fragment);
        if (vert && frag) {
            VertexLayout skybox_layout;
            skybox_layout.stride = sizeof(float) * 3;
            skybox_layout.attributes = {
                { 0, 0, 0, TextureFormat::RGB32F },
            };
            cache.get_or_create_graphics("skybox", GraphicsPipelineDesc{
                .vertex_shader = vert,
                .fragment_shader = frag,
                .vertex_layout = skybox_layout,
                .state = {
                    .depth_write = false,
                    .depth_test = true,
                    .depth_compare = CompareOp::LessOrEqual,
                    .cull_mode = CullMode::None,
                },
                .debug_name = "SkyboxPipeline",
            });
        }
    }

    // Tonemap compute pipeline
    {
        auto* comp = cache.load_shader("shaders/tonemap.comp.spv",
                                        ShaderStage::Compute);
        if (comp) {
            cache.get_or_create_compute("tonemap_compute", ComputePipelineDesc{
                .compute_shader = comp,
                .push_constant_size = sizeof(TonemapPushConstants),
                .debug_name = "TonemapComputePipeline",
            });
        }
    }
}
```

- [ ] **Step 2: Call initialize_pipelines from ForwardPlusPlugin::build()**

```cpp
void ForwardPlusPlugin::build(App& app) {
    app.insert_resource(config);

    // Create and register PipelineCache and DefaultTextures resources
    auto& device = app.resource<rhi::Device>();
    app.insert_resource(DefaultTextures(device));
    app.insert_resource(PipelineCache(device));

    // Pre-create all rendering pipelines
    auto& cache = app.resource<PipelineCache>();
    initialize_pipelines(device, cache);

    // Initialize FramePacket resource
    app.insert_resource(FramePacket{});

    app.add_system(Schedule::PreRender, extract_render_data);
    app.add_system(Schedule::PreRender, build_forward_plus_graph
        .after(extract_render_data));
}
```

- [ ] **Step 3: Verify all pipeline names match the names used in pass execute lambdas**

---

## Task 18: Tests

**Files:**
- Create: `helios-renderer/tests/test_forward_plus.cpp`

- [ ] **Step 1: Test extract_render_data produces correct FramePacket**

```cpp
#include <gtest/gtest.h>
#include "helios-core/src/ecs/world.h"
#include "helios-renderer/src/forward_plus/forward_plus_plugin.h"
#include "helios-renderer/src/frame_packet.h"

using namespace helios;

TEST(ForwardPlus, ExtractRenderData_ProducesCorrectPacket) {
    World world;

    // Insert FramePacket resource
    world.insert_resource(FramePacket{});

    // Spawn camera entity
    auto cam = world.spawn();
    world.add(cam, Transform{ .position = {0, 5, 10} });
    world.add(cam, Camera{ .fov_y = 60.0f, .near_plane = 0.1f, .far_plane = 100.0f });
    world.add(cam, ActiveCamera{});

    // Spawn mesh entity
    auto mesh = world.spawn();
    world.add(mesh, Transform{ .position = {1, 2, 3} });
    world.add(mesh, MeshRenderer{ .mesh = AssetHandle{42}, .material = AssetHandle{7} });

    // Spawn disabled mesh (should be excluded)
    auto disabled = world.spawn();
    world.add(disabled, Transform{});
    world.add(disabled, MeshRenderer{ .mesh = AssetHandle{99} });
    world.add(disabled, Disabled{});

    // Spawn point light
    auto light = world.spawn();
    world.add(light, Transform{ .position = {5, 5, 5} });
    world.add(light, PointLight{ .color = {1, 0, 0}, .intensity = 2.0f, .radius = 15.0f });

    // Spawn directional light
    auto dir_light = world.spawn();
    world.add(dir_light, Transform{});
    world.add(dir_light, DirectionalLight{
        .color = {1, 1, 1}, .intensity = 1.5f,
        .direction = {0, -1, 0}, .cast_shadows = true,
    });

    // Run the extract system
    world.run_system(extract_render_data);

    auto& packet = world.resource<FramePacket>();

    // Camera
    EXPECT_FLOAT_EQ(packet.camera.near_plane, 0.1f);
    EXPECT_FLOAT_EQ(packet.camera.far_plane, 100.0f);
    EXPECT_FLOAT_EQ(packet.camera.fov_y, 60.0f);
    EXPECT_NEAR(packet.camera.position.x, 0.0f, 0.001f);
    EXPECT_NEAR(packet.camera.position.y, 5.0f, 0.001f);

    // Meshes (should have 1, not 2 -- disabled entity excluded)
    ASSERT_EQ(packet.mesh_draws.size(), 1u);
    EXPECT_EQ(packet.mesh_draws[0].mesh.id, 42u);
    EXPECT_EQ(packet.mesh_draws[0].material.id, 7u);

    // Point lights
    ASSERT_EQ(packet.point_lights.size(), 1u);
    EXPECT_FLOAT_EQ(packet.point_lights[0].intensity, 2.0f);
    EXPECT_FLOAT_EQ(packet.point_lights[0].radius, 15.0f);

    // Directional lights
    ASSERT_EQ(packet.dir_lights.size(), 1u);
    EXPECT_FLOAT_EQ(packet.dir_lights[0].intensity, 1.5f);
    EXPECT_TRUE(packet.dir_lights[0].cast_shadows);
}
```

- [ ] **Step 2: Test render graph builds expected pass order**

```cpp
TEST(ForwardPlus, GraphBuild_ProducesCorrectPassOrder) {
    // Create a mock/stub RenderGraph that records pass names
    TestRenderGraph graph;

    FramePacket packet;
    packet.viewport_width = 1280;
    packet.viewport_height = 720;
    packet.camera = CameraData{
        .view = glm::mat4(1.0f),
        .projection = glm::perspective(glm::radians(45.0f), 16.0f/9.0f, 0.1f, 500.0f),
        .position = {0, 0, 0},
        .near_plane = 0.1f,
        .far_plane = 500.0f,
        .fov_y = 45.0f,
        .aspect_ratio = 16.0f / 9.0f,
    };
    packet.dir_lights.push_back(DirLightData{
        .direction = {0, -1, 0},
        .cast_shadows = true,
    });
    packet.point_lights.push_back(LightData{
        .position = {5, 5, 5},
        .intensity = 1.0f,
    });

    ForwardPlusConfig config;
    // Use a stub PipelineCache
    StubPipelineCache cache;

    // Simulate what build_forward_plus_graph does
    auto depth = add_depth_prepass(graph, packet, cache);
    auto shadows = add_shadow_pass(graph, packet, config, cache);
    auto light_cull = add_light_culling_pass(graph, depth, packet, config, cache);
    auto hdr = add_forward_pass(graph, depth, shadows, light_cull, packet, config, cache);
    auto skybox = add_skybox_pass(graph, hdr, packet, cache);
    auto ldr = add_tonemap_pass(graph, skybox, packet, config, cache);
    graph.set_output(ldr);

    // Verify pass names in registration order
    auto names = graph.get_pass_names();
    ASSERT_EQ(names.size(), 6u);
    EXPECT_EQ(names[0], "DepthPrepass");
    EXPECT_EQ(names[1], "ShadowPass");
    EXPECT_EQ(names[2], "LightCulling");
    EXPECT_EQ(names[3], "ForwardPass");
    EXPECT_EQ(names[4], "SkyboxPass");
    EXPECT_EQ(names[5], "TonemapPass");

    // Verify graph has a valid output
    EXPECT_TRUE(ldr.index != 0);
}
```

- [ ] **Step 3: Test cascade matrix computation**

```cpp
TEST(ForwardPlus, CascadeMatrices_CorrectCount) {
    CameraData camera{
        .view = glm::lookAt(glm::vec3(0, 0, 5), glm::vec3(0), glm::vec3(0, 1, 0)),
        .projection = glm::perspective(glm::radians(45.0f), 16.0f/9.0f, 0.1f, 500.0f),
        .near_plane = 0.1f,
        .far_plane = 500.0f,
        .fov_y = 45.0f,
        .aspect_ratio = 16.0f / 9.0f,
    };

    auto splits = compute_cascade_splits(0.1f, 500.0f, 4);
    EXPECT_EQ(splits.size(), 3u); // 4 cascades = 3 split points

    auto matrices = compute_cascade_matrices(camera, glm::vec3(0, -1, 0), splits);
    EXPECT_EQ(matrices.size(), 4u); // one matrix per cascade

    // Each matrix should be a valid (non-zero) projection
    for (const auto& m : matrices) {
        EXPECT_NE(glm::determinant(m), 0.0f);
    }
}
```

- [ ] **Step 4: Test DefaultTextures construction**

```cpp
TEST(ForwardPlus, DefaultTextures_ConstructsAllTextures) {
    // Requires a valid RHI device (integration test)
    // Use a test device fixture
    TestDevice device;
    DefaultTextures defaults(device);

    EXPECT_TRUE(static_cast<bool>(defaults.white()));
    EXPECT_TRUE(static_cast<bool>(defaults.black()));
    EXPECT_TRUE(static_cast<bool>(defaults.blue()));
    EXPECT_TRUE(static_cast<bool>(defaults.black_cube()));
    EXPECT_TRUE(static_cast<bool>(defaults.white_array()));
    EXPECT_TRUE(static_cast<bool>(defaults.brdf_lut_placeholder()));
}
```

- [ ] **Step 5: Test IBL generation produces valid textures**

```cpp
TEST(ForwardPlus, IBLGeneration_ProducesTextures) {
    // Integration test requiring GPU
    TestDevice device;
    PipelineCache cache(device);

    // Create a simple 4x4 cubemap for testing
    uint32_t face_data[6 * 4 * 4];
    std::fill(std::begin(face_data), std::end(face_data), 0xFF804020);
    rhi::Texture env_cube(device, TextureDesc{
        .width = 4, .height = 4,
        .format = TextureFormat::RGBA8,
        .type = TextureType::TextureCube,
        .mip_levels = 1,
        .array_layers = 6,
        .usage = TextureUsage::Sampled | TextureUsage::Transfer,
    }, face_data);

    auto ibl = generate_ibl(device, cache, env_cube,
        /*irradiance=*/4, /*prefilter=*/4, /*brdf=*/16);

    EXPECT_TRUE(static_cast<bool>(ibl.irradiance_map));
    EXPECT_TRUE(static_cast<bool>(ibl.prefilter_map));
    EXPECT_TRUE(static_cast<bool>(ibl.brdf_lut));

    EXPECT_EQ(ibl.irradiance_map.width(), 4u);
    EXPECT_EQ(ibl.prefilter_map.width(), 4u);
    EXPECT_EQ(ibl.brdf_lut.width(), 16u);
}
```

- [ ] **Step 6: Add CMakeLists.txt test target**

```cmake
# In helios-renderer/tests/CMakeLists.txt
add_executable(test_forward_plus test_forward_plus.cpp)
target_link_libraries(test_forward_plus PRIVATE
    helios-renderer
    helios-core
    GTest::gtest_main
)
add_test(NAME ForwardPlusTests COMMAND test_forward_plus)
```

---

## Summary: Pass Execution Order

When `build_forward_plus_graph` runs, the render graph is populated with this dependency chain:

```
DepthPrepass  ──────────────────┐
                                ├──> LightCulling ──┐
ShadowPass   ──────────────────>│                    ├──> ForwardPass ──> SkyboxPass ──> TonemapPass ──> OUTPUT
                                                     │
(depth texture read by culling)                      │
(shadow map read by forward)  ──────────────────────>┘
```

The render graph compiler:
1. Walks backwards from `set_output(ldr)` to identify needed passes
2. Topologically sorts by resource dependencies
3. Inserts barriers (depth attachment -> shader read, compute write -> fragment read, etc.)
4. Allocates transient textures/buffers from a pool
5. Executes passes in order on the render thread
