# Rendering Pipeline

Helios features a modern, high-performance rendering stack built on Vulkan 1.3. The architecture is designed for scalability and efficiency, utilizing a Forward+ rendering path for dynamic lighting and a robust abstraction layer to manage GPU resources safely.

## Architecture Overview

The rendering stack is split into three distinct layers:

1.  **RHI (Render Hardware Interface)** (`helios/rhi/`): A stateless, abstract GPU interface that provides a unified way to manage resources (textures, buffers, pipelines) without Vulkan boilerplate.
2.  **Vulkan Backend** (`helios/vulkan/`): A fully-featured implementation of the RHI using modern Vulkan best practices, including dynamic rendering, synchronization 2, and VMA for memory management.
3.  **Forward+ Pipeline** (`helios/forward_plus/`): The default rendering pipeline. It utilizes compute shaders for light culling, allowing for hundreds of point lights per scene with high performance.

## RHI Abstraction

The RHI lives in the `helios::rhi` namespace. It treats all GPU objects as opaque resources returned via `std::unique_ptr`. Raw Vulkan types are strictly encapsulated within the backend and never leak into the engine or application code.

### Resource Lifecycle & Pacing

To maximize GPU throughput while ensuring safety, Helios employs a triple-buffering strategy for transient state.

- **`MAX_FRAMES_IN_FLIGHT = 3`**: This constant, defined in `rhi::Device`, dictates how many frames the GPU can work on simultaneously while the CPU prepares the next.
- **Deferred Deletion**: GPU resources cannot be destroyed while in-flight. Use `device->defer_destroy(std::move(resource))` to safely queue a resource for deletion after `MAX_FRAMES_IN_FLIGHT` frames.

### Key Resource Types

| Type | Description |
| :--- | :--- |
| `Device` | The central factory for all GPU resources and command submission. |
| `Texture` | Manages 2D, Cube, and Array textures with automated layout transitions. |
| `Buffer` | Unified interface for vertex, index, uniform, and storage buffers. |
| `Pipeline` | Encapsulates graphics or compute state, avoiding redundant state changes. |
| `CommandBuffer` | Records GPU commands for submission to the queue. |
| `DescriptorSet` | Manages shader resource binding (textures, buffers). |

## Render Context & Settings

The renderer is integrated into the ECS via two primary resources: **[`RenderContext`](../api/renderer/render_context.md)** and **[`RenderSettings`](../api/renderer/render_settings.md)**.

### `RenderContext`
The **[`RenderContext`](../api/renderer/render_context.md)** resource holds the lifetime of the GPU device, the swapchain, and the per-frame command buffers. It also manages the scene framebuffers and per-camera render targets.

Systems interacting with the renderer should use the `ResMut<RenderContext>` pattern:

```cpp
void my_render_system(ResMut<RenderContext> ctx) {
    auto* device = ctx->device.get(); // Access low-level RHI
    auto* cmd = ctx->cmd;             // Current frame command buffer
    // ...
}
```

### `RenderMode`
The global rendering mode is configured via `RenderPlugin` and reflected in `RenderContext::mode`:

- **`RenderMode::Direct`**: The default mode. The internal scene framebuffer is automatically blitted to the swapchain at the end of the frame for display.
- **`RenderMode::Offscreen`**: The scene is rendered to the internal framebuffer but not blitted to the swapchain. This is used by the Editor to display the scene within an ImGui window.

### `RenderSettings`

Centralized, runtime-changeable **[`settings`](../api/renderer/render_settings.md)** that govern the global render state. These settings are typically accessed via `ResMut<RenderSettings>` within a system.

- **VSync**: Controlled via `present_mode`. Use `set_vsync(bool)` for a simplified interface (`Fifo` for VSync, `Immediate` for off).
- **Resolution Scale**: Dynamically scales the internal render resolution (0.25x to 2.0x) without resizing the window.
- **Exposure & Ambient Lighting**: Global `exposure` control for tonemapping, and `ambient_color` / `ambient_intensity` for basic environmental fill lighting.
- **Shadow Settings**: Configure `shadow_resolution` and `shadow_cascades` for directional lights. Updating these may trigger a lazy reallocation of shadow map resources.
- **Tonemapping**: Selection of operators: `ACES` (default), `Reinhard`, or `Filmic`. Set to `None` for raw HDR output.
- **Debug Visualization**: The `debug_draw` field supports bitwise flags for various visualizations. Use `toggle_debug(flag)` or `has_debug(flag)` to manage:
    - `Wireframe`: Renders geometry in wireframe mode.
    - `Colliders`: Displays physics collider geometry.
    - `Normals`: Shows surface normals for debugging mesh data.
    - `LightBounds`: Visualizes the screen-space tiles and point light influence.
    - `BoundingBoxes`: Shows AABBs for all renderable entities.

#### Example Usage

```cpp
void adjust_settings(ResMut<RenderSettings> settings) {
    // Enable VSync
    settings->set_vsync(true);

    // Boost ambient lighting for a daylight look
    settings->ambient_intensity = 0.5f;
    settings->ambient_color = glm::vec3(1.0f, 0.9f, 0.8f);

    // Enable debug visualizations
    settings->toggle_debug(DebugDraw::Wireframe | DebugDraw::Colliders);

    // Increase shadow quality (triggers a resource refresh)
    settings->set_shadow_resolution(4096);
    settings->shadow_cascades = 4;

    // Switch tonemapper
    settings->tonemap = TonemapMode::Filmic;
}
```

## Camera System

Rendering is driven by entities with the `Camera` and `ActiveCamera` components.

### `ActiveCamera` Tag
The `ActiveCamera` marker component is essential; only cameras with this tag are processed by the rendering pipeline. In the editor, this tag is dynamically managed to switch between the Editor Camera and game-view cameras.

### `Camera` Component
Defines the projection (Perspective or Orthographic), FOV, clipping planes, and clear behavior. Each camera also specifies a `render_schedule` (e.g., "forward_plus"), allowing different cameras to use different rendering techniques.

### Camera Driver
The `camera_driver` system executes during `PreRender`. It gathers all active cameras, sorts them by their `order` property, and executes their associated render schedules.

## Forward+ Pipeline

The `ForwardPlusPlugin` provides the engine's primary rendering path. It is a high-performance solution that handles many light sources by using a compute-based light culling pass.

!!! note "Anti-Aliasing"
    MSAA (Multi-Sample Anti-Aliasing) is not currently supported in the Forward+ pipeline. Improved anti-aliasing techniques are on the roadmap for future development.

### Configuration (`ForwardPlusConfig`)
The pipeline is configured via the `ForwardPlusConfig` resource, typically set during application startup:
- **`skybox_hdr_path`**: The path to the HDR environment map used for the skybox and IBL (Image Based Lighting). This is the only way to configure the skybox; it is not currently an ECS component.
- **`tile_size`**: The size of the screen-space tiles used for light culling (default 16x16).
- **`shadow_resolution`**: The resolution of the cascaded shadow maps.

### Pipeline Stages

The pipeline executes the following stages in sequence:

1.  **Extract**: Gathers all mesh renderers and lights into a `FramePacket`.
2.  **Depth Prepass**: Renders geometry to the depth buffer first. This reduces overdraw in the forward pass and provides the depth information needed for light culling.
3.  **Shadow Pass**: Generates cascaded shadow maps for directional lights.
4.  **Light Culling**: A compute shader tiles the screen and culls the list of point lights against the Z-bounds of each tile.
5.  **Forward Pass**: The main PBR shading pass. It uses the tiled light lists from the culling stage to shade geometry efficiently.
6.  **Skybox Pass**: Renders the environment cubemap behind the geometry.
7.  **Tonemapping**: Final pass that converts the HDR scene buffer to the display's LDR range.

## Scene Setup

To render a mesh, you must spawn an entity with `Transform` and `MeshRenderer` components. The `MeshRenderer` requires a `Handle` to a `MeshAsset` and optionally a `MaterialAsset`.

```cpp
void setup_scene(App& app, Res<AssetServer> assets) {
    // Load assets
    auto mesh = assets->load<MeshAsset>("Meshes/Sphere.hvemesh");
    auto material = assets->load<MaterialAsset>("Materials/Gold.hvemat");

    // Spawn entity
    app.spawn()
        .add<Transform>({ .position = glm::vec3(0, 0, 0) })
        .add<MeshRenderer>({
            .mesh = mesh,
            .material = material // Overrides the mesh's default material
        });
}
```

## Lighting

Lighting is integrated into the ECS with support for various light types:

- **`DirectionalLight`**: A global, distant light source with support for cascaded shadows.
- **`PointLight`**: An omnidirectional light with a specific radius.
- **`SpotLight`**: *Note: Currently, SpotLights are internal-only and not yet exposed as ECS components.*

### Light Components

```cpp
// Create a bright white directional light
app.spawn()
    .add<Transform>({ .rotation = glm::quatLookAt(glm::vec3(-1.0f, -1.0f, -1.0f), glm::vec3(0, 1, 0)) })
    .add<DirectionalLight>({
        .color = glm::vec3(1.0f),
        .intensity = 5.0f
    });

// Create a small red point light
app.spawn()
    .add<Transform>({ .position = glm::vec3(0, 5, 0) })
    .add<PointLight>({
        .color = glm::vec3(1.0f, 0.0f, 0.0f),
        .intensity = 10.0f,
        .radius = 25.0f
    });
```

### Render Layers
Both cameras and renderable entities (meshes, lights) support `RenderLayers`. A camera only renders an entity if their layer masks intersect, providing a flexible way to partition scenes or create specialized view effects.
