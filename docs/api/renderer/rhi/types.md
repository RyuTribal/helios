# RHI Types

This page documents the common enums and structures used across the Helios Renderer Hardware Interface (RHI).

## Enums

### TextureFormat
Supported formats for textures and attachments.
- `R8`, `RG8`, `RGBA8`, `BGRA8`
- `RG16F`, `RGBA16F`
- `R32F`, `RG32F`, `RGB32F`, `RGBA32F`
- `Depth32F`, `Depth24Stencil8`

### BufferUsage
Flags indicating how a buffer can be used.
- `Vertex`, `Index`, `Uniform`, `Storage`, `Transfer`

### TextureUsage
Flags indicating how a texture can be used.
- `Sampled`, `Storage`, `ColorAttachment`, `DepthAttachment`, `Transfer`

### MemoryAccess
Determines where a resource is stored and how it can be accessed.
- `GPU_Only`: Fastest performance, requires command buffers for updates.
- `CPU_to_GPU`: Host-visible memory, typically used for dynamic buffers.
- `GPU_to_CPU`: Used for readbacks from the GPU.

### PresentMode
Determines how frames are presented to the screen.
- `Immediate`: No VSync, potential tearing.
- `Fifo`: Standard VSync.
- `Mailbox`: Triple-buffering behavior.

## Structures

### TextureDesc
Configuration for creating an `rhi::Texture`.
- `width`, `height`: Pixel dimensions.
- `format`: `TextureFormat`.
- `usage`: `TextureUsage` flags.
- `sampler`: `SamplerMode` (Repeat, ClampToEdge, etc.).

### BufferDesc
Configuration for creating an `rhi::Buffer`.
- `size`: Buffer size in bytes.
- `usage`: `BufferUsage` flags.
- `access`: `MemoryAccess`.

### GraphicsPipelineDesc
Description of the entire graphics pipeline state.
- `vertex_shader`, `fragment_shader`: Required shaders.
- `layout`: `VertexLayout` definition.
- `state`: `RenderState` (culling, depth test, blending).
- `render_pass`: The render pass this pipeline is compatible with.
- `descriptor_layouts`: List of descriptor sets used by this pipeline.

### ClearValues
Values used when clearing attachments at the start of a pass.
- `color[4]`: RGBA color values.
- `depth`: Depth clear value (default 1.0f).
- `stencil`: Stencil clear value (default 0).

## Related Pages
- [rhi::Device](device.md)
- [rhi::CommandBuffer](command_buffer.md)
- [rhi::Texture](texture.md)
- [rhi::Buffer](buffer.md)
