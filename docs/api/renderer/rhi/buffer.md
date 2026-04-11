# rhi::Buffer

The `rhi::Buffer` class represents a GPU memory buffer used for storing data like vertices, indices, or uniform/storage data.

## Member Functions

| Method | Description |
| :--- | :--- |
| `set_data(data, size, offset)` | Updates the buffer's contents from the CPU. |
| `map()` | Maps the buffer memory into CPU address space. |
| `unmap()` | Unmaps the buffer memory. |
| `size()` | Returns the buffer's size in bytes. |
| `desc()` | Returns the original `BufferDesc`. |
| `native_handle<T>()` | Returns the backend-specific handle (e.g., `VkBuffer`). |

## Usage Example

```cpp
rhi::BufferDesc desc;
desc.size = vertices.size() * sizeof(Vertex);
desc.usage = rhi::BufferUsage::Vertex;
desc.access = rhi::MemoryAccess::CPU_to_GPU;

auto buffer = device->create_buffer(desc);

// Updating data
buffer->set_data(vertices.data(), desc.size);

// Mapping for manual updates
void* ptr = buffer->map();
std::memcpy(ptr, data, size);
buffer->unmap();
```

## Related Pages
- [rhi::Device](device.md)
- [rhi::CommandBuffer](command_buffer.md)
- [RHI Types](types.md)
