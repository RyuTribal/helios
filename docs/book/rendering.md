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

The renderer is integrated into the ECS via two primary resources: `RenderContext` and `RenderSettings`.

### `RenderContext`
The `RenderContext` resource holds the lifetime of the GPU device, the swapchain, and the per-frame command buffers. It also manages the scene framebuffers and per-camera render targets.

### `RenderSettings`
Centralized, runtime-changeable settings that govern the global render state:
- **VSync**: Controlled via `present_mode` (e.g., `Fifo` for VSync, `Immediate` for off).
- **Resolution Scale**: Dynamically scales the internal render resolution (0.25x to 2.0x).
- **Tonemapping**: Selection of operators including ACES, Reinhard, and Filmic.
- **Debug Visualization**: Flags for wireframe, colliders, normals, and light bounds.

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

### Configuration (`ForwardPlusConfig`)
The pipeline is configured via the `ForwardPlusConfig` resource, typically set during application startup:
- **`skybox_hdr_path`**: The path to the HDR environment map used for the skybox and IBL (Image Based Lighting).
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

## Lighting

Lighting is integrated into the ECS with support for various light types:

- **`DirectionalLight`**: A global, distant light source with support for cascaded shadows.
- **`PointLight`**: An omnidirectional light with a specific radius.
- **`SpotLight`**: *Note: Currently, SpotLights are only supported within the renderer's internal `FramePacket` structures and are not yet available as high-level ECS components.*

### Render Layers
Both cameras and renderable entities (meshes, lights) support `RenderLayers`. A camera only renders an entity if their layer masks intersect, providing a flexible way to partition scenes or create specialized view effects.
