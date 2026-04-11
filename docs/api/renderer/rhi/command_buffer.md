# rhi::CommandBuffer

The `rhi::CommandBuffer` class is used to record GPU commands, such as drawing, pipeline binding, and resource transitions.

## Member Functions

### Recording

| Method | Description |
| :--- | :--- |
| `begin()` | Starts recording commands. |
| `end()` | Finishes recording commands. |

### Rendering

| Method | Description |
| :--- | :--- |
| `begin_rendering(color, depth, clear, width, height)` | Starts a dynamic rendering pass. |
| `end_rendering()` | Finishes a dynamic rendering pass. |
| `begin_render_pass(render_pass, framebuffer, clear)` | Starts a legacy render pass. |
| `end_render_pass()` | Finishes a legacy render pass. |

### Pipeline and State

| Method | Description |
| :--- | :--- |
| `bind_pipeline(pipeline)` | Binds a graphics or compute pipeline. |
| `set_viewport(x, y, w, h)` | Sets the viewport state. |
| `set_scissor(x, y, w, h)` | Sets the scissor state. |

### Resource Binding

| Method | Description |
| :--- | :--- |
| `bind_vertex_buffer(buffer, binding)` | Binds a vertex buffer for drawing. |
| `bind_index_buffer(buffer, type)` | Binds an index buffer for indexed drawing. |
| `bind_descriptor_set(set, ds)` | Binds a descriptor set (textures/buffers). |
| `push_constants(stage, offset, size, data)` | Updates push constant data. |

### Commands

| Method | Description |
| :--- | :--- |
| `draw(vertex_count, instances, first)` | Performs a non-indexed draw. |
| `draw_indexed(index_count, instances, first)` | Performs an indexed draw. |
| `dispatch(x, y, z)` | Dispatches a compute workload. |
| `clear_image(texture, r, g, b, a)` | Clears a color texture. |

### Synchronization and Transfer

| Method | Description |
| :--- | :--- |
| `pipeline_barrier(barrier)` | Injects a pipeline barrier. |
| `transition_image(texture, old, new)` | Transitions a texture layout. |
| `copy_buffer(src, dst, size)` | Copies data between buffers. |
| `blit_image(src, dst, ...)` | Scales and copies between textures. |

## Usage Example

```cpp
cmd->begin();
cmd->transition_image(texture.get(), rhi::TextureLayout::Undefined, rhi::TextureLayout::ColorAttachment);
cmd->begin_rendering(texture.get(), nullptr, clear_values, width, height);
cmd->bind_pipeline(pipeline.get());
cmd->draw(3);
cmd->end_rendering();
cmd->transition_image(texture.get(), rhi::TextureLayout::ColorAttachment, rhi::TextureLayout::PresentSrc);
cmd->end();
```

## Related Pages
- [rhi::Device](device.md)
- [rhi::Texture](texture.md)
- [rhi::Buffer](buffer.md)
- [RHI Types](types.md)
