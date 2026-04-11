# RenderPlugin

The `RenderPlugin` is the entry point for initializing the Helios rendering engine. It sets up the `RenderContext` and `RenderSettings` and configures the underlying graphics API.

## Member Variables

| Member | Description | Default |
| :--- | :--- | :--- |
| `backend` | The RHI backend to use (`Vulkan`). | `Vulkan` |
| `gpu_index` | Which GPU to use (UINT32_MAX = automatic). | `UINT32_MAX` |
| `app_name` | The name of the application. | `"Helios"` |
| `enable_validation` | Enables graphics API validation layers. | `true` |
| `mode` | Determines how the final image is presented. | `Direct` |
| `initial_present_mode` | The starting VSync mode (`Fifo`). | `Fifo` |

## Member Functions

| Method | Description |
| :--- | :--- |
| `build(App& app)` | Injects the rendering infrastructure into the application. |

## Usage Example

```cpp
#include "helios/render_plugin.h"

App::create()
    .add_plugin(RenderPlugin{
        .app_name = "MyGame",
        .enable_validation = false
    })
    .run();
```

## Related Pages
- [RenderContext](render_context.md)
- [RenderSettings](render_settings.md)
- [RHI Device](rhi/device.md)
