# RenderSettings

The `RenderSettings` struct is a resource stored in the ECS that manages runtime-changeable rendering parameters, such as VSync, shadow resolution, and debug visualizations.

## Member Variables

### Swapchain & Resolution

| Member | Description |
| :--- | :--- |
| `present_mode` | The current VSync mode (`rhi::PresentMode`). |
| `resolution_scale` | The multiplier for the scene framebuffer resolution (0.25 - 2.0). |

### Post-Processing

| Member | Description |
| :--- | :--- |
| `clear_color` | The background color (RGBA). |
| `exposure` | The exposure level for HDR. |
| `tonemap` | The tonemapping operator to use (`TonemapMode`). |

### Quality Settings

| Member | Description |
| :--- | :--- |
| `shadow_resolution` | The pixel dimension for shadow maps. |
| `shadow_cascades` | The number of shadow cascades for directional lights. |
| `max_point_lights` | The limit for tiled lighting calculations. |

### Debugging

| Member | Description |
| :--- | :--- |
| `debug_draw` | Bitfield for debug visualization overlays. |

## DebugDraw Flags

The `DebugDraw` enum is a bitfield that can enable multiple overlays simultaneously:

- `None`: No debug visualization.
- `Wireframe`: Renders the scene in wireframe mode.
- `Colliders`: Displays physics colliders.
- `LightBounds`: Visualizes the reach of point and spot lights.
- `Normals`: Shows mesh normals.
- `BoundingBoxes`: Renders entity bounding boxes.

## Member Functions

| Method | Description |
| :--- | :--- |
| `set_vsync(on)` | Updates `present_mode` to `Fifo` or `Immediate`. |
| `vsync()` | Checks if VSync is enabled. |
| `set_shadow_resolution(res)` | Updates the shadow map size and marks the state dirty. |
| `toggle_debug(flag)` | Toggles a specific `DebugDraw` flag. |
| `has_debug(flag)` | Returns true if the debug flag is enabled. |

## Usage Example

```cpp
void adjust_settings(ResMut<RenderSettings> settings) {
    // Toggling VSync
    settings->set_vsync(true);

    // Activating wireframe and collider visualization
    settings->debug_draw = DebugDraw::Wireframe | DebugDraw::Colliders;

    // Increasing render resolution for high-end GPUs
    settings->resolution_scale = 1.5f;
}
```

## Related Pages
- [RenderPlugin](render_plugin.md)
- [RenderContext](render_context.md)
- [RHI Types](rhi/types.md)
