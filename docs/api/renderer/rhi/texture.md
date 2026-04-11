# rhi::Texture

The `rhi::Texture` class represents a GPU texture resource, which can be used as a color/depth attachment, a sampled image in a shader, or a storage image for compute tasks.

## Member Functions

| Method | Description |
| :--- | :--- |
| `width()` | Returns the texture width in pixels. |
| `height()` | Returns the texture height in pixels. |
| `format()` | Returns the `TextureFormat`. |
| `desc()` | Returns the original `TextureDesc` used to create the texture. |
| `native_handle<T>()` | Returns the backend-specific handle (e.g., `VkImage`). |

## Usage Example

```cpp
rhi::TextureDesc desc;
desc.width = 1920;
desc.height = 1080;
desc.format = rhi::TextureFormat::RGBA8;
desc.usage = rhi::TextureUsage::Sampled | rhi::TextureUsage::ColorAttachment;

auto texture = device->create_texture(desc);

uint32_t width = texture->width();
```

## Related Pages
- [rhi::Device](device.md)
- [rhi::CommandBuffer](command_buffer.md)
- [RHI Types](types.md)
