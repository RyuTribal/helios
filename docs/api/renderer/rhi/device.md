# rhi::Device

The `rhi::Device` class is the primary interface for interacting with the GPU. It acts as a factory for all GPU resources (textures, buffers, shaders, pipelines) and manages the command submission queue.

## Member Functions

### Resource Factory

| Method | Description |
| :--- | :--- |
| `create_texture(desc, data)` | Creates a GPU texture. |
| `create_buffer(desc, data)` | Creates a GPU buffer (Vertex, Index, Uniform, etc.). |
| `create_shader(desc)` | Compiles SPIR-V code into a shader module. |
| `create_graphics_pipeline(desc)` | Creates a graphics pipeline state object. |
| `create_compute_pipeline(desc)` | Creates a compute pipeline state object. |
| `create_command_buffer()` | Allocates a new command buffer for recording. |
| `create_render_pass(desc)` | Defines a set of attachments and subpasses. |
| `create_framebuffer(desc)` | Binds textures to a render pass. |

### Descriptor Management

| Method | Description |
| :--- | :--- |
| `create_descriptor_set_layout(desc)` | Defines the layout of a descriptor set. |
| `allocate_descriptor_set(layout)` | Allocates a descriptor set from the device pool. |
| `update_descriptor_set(set, writes)` | Updates the contents of a descriptor set. |

### Surface & Swapchain

| Method | Description |
| :--- | :--- |
| `create_surface(window_handle)` | Creates an RHI surface for window integration. |
| `destroy_surface(surface)` | Destroys an RHI surface. |
| `create_swapchain(desc)` | Creates a swapchain for window presentation. |

### Command Submission

| Method | Description |
| :--- | :--- |
| `submit(cmd, info)` | Submits a command buffer to the GPU queue. |
| `submit_for_present(cmd, swapchain)` | Submits a command buffer with automatic swapchain synchronization. |
| `wait_idle()` | Blocks the CPU until all GPU work is complete. |

### Lifetime Management

| Method | Description |
| :--- | :--- |
| `defer_destroy(resource)` | Queues a resource for deletion after `MAX_FRAMES_IN_FLIGHT`. |
| `flush_deferred_deletions()` | Destroys resources that have completed their lifecycle. |

## Usage Example

```cpp
// Creating a vertex buffer
rhi::BufferDesc desc;
desc.size = sizeof(vertices);
desc.usage = rhi::BufferUsage::Vertex;
desc.access = rhi::MemoryAccess::GPU_Only;

auto vertex_buffer = device->create_buffer(desc, vertices.data());

// Deferring destruction safely
device->defer_destroy(std::move(old_texture));
```

## Related Pages
- [rhi::CommandBuffer](command_buffer.md)
- [rhi::Texture](texture.md)
- [rhi::Buffer](buffer.md)
- [RHI Types](types.md)
