# RenderContext

The `RenderContext` is a resource stored in the ECS that provides global access to the RHI device, swapchain, and current frame's scene textures.

## Member Variables

### RHI Objects

| Member | Description |
| :--- | :--- |
| `device` | The `rhi::Device` used to create all resources. |
| `swapchain` | The `rhi::Swapchain` for window presentation. |
| `cmd` | The `rhi::CommandBuffer` for the CURRENT frame. |

### Scene Data

| Member | Description |
| :--- | :--- |
| `scene_color` | Pointer to the current frame's scene color texture. |
| `scene_depth` | Pointer to the current frame's scene depth texture. |
| `scene_width` | Width of the scene framebuffer. |
| `scene_height` | Height of the scene framebuffer. |
| `current_frame_index` | The current frame being recorded (0 to `MAX_FRAMES_IN_FLIGHT - 1`). |

## Member Functions

| Method | Description |
| :--- | :--- |
| `get_or_create_target(id, w, h)` | Returns or creates a custom render target for a camera. |
| `resize(w, h, present_mode, scale)` | Recreates the swapchain and scene framebuffers. |
| `resize_scene_fb(w, h)` | Resizes the scene framebuffer only. |

## Usage Example

```cpp
void my_render_system(ResMut<RenderContext> ctx) {
    // Accessing the current frame's command buffer
    auto* cmd = ctx->cmd;

    // Transitioning the scene color for rendering
    cmd->transition_image(*ctx->scene_color, rhi::TextureLayout::ShaderReadOnly, rhi::TextureLayout::ColorAttachment);

    // Using the device to create transient resources
    auto buffer = ctx->device->create_buffer(desc);
}
```

## Related Pages
- [RenderPlugin](render_plugin.md)
- [RenderSettings](render_settings.md)
- [RHI Device](rhi/device.md)
- [RHI Command Buffer](rhi/command_buffer.md)
