# Helios Renderer API Reference

Welcome to the Helios Renderer API reference. This section provides detailed documentation for the renderer's core components, high-level settings, and the underlying Hardware Interface (RHI).

## High-Level API

The high-level API is integrated into the Helios ECS and provides easy-to-use resources and components for scene rendering.

- [RenderSettings](render_settings.md): Global rendering parameters, post-processing, and quality settings.
- [RenderContext](render_context.md): The main interface for high-level render commands.
- [RenderPlugin](render_plugin.md): The ECS plugin that initializes the renderer.

## Renderer Hardware Interface (RHI)

The RHI is a low-level abstraction over modern graphics APIs (primarily Vulkan). It provides direct control over GPU resources and command submission.

- [rhi::Device](rhi/device.md): The primary factory for GPU resources.
- [rhi::CommandBuffer](rhi/command_buffer.md): Recording and submitting GPU commands.
- [rhi::Texture](rhi/texture.md): GPU texture and image resources.
- [rhi::Buffer](rhi/buffer.md): GPU memory buffers for vertex, index, and uniform data.
- [RHI Types](rhi/types.md): Common structures and enumerations used across the RHI.

## Key Features

- **Modern Graphics:** Built on Vulkan with support for dynamic rendering and bindless descriptors.
- **Clustered Lighting:** Efficiently handle hundreds of light sources.
- **Post-Processing:** Built-in HDR, tonemapping, and bloom.
- **Extensible:** Create custom render passes and pipeline states through the RHI.
