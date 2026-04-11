# Helios Engine Documentation

**v0.5.0** — A modular, data-driven game engine built with C++20 and Vulkan 1.3.

## The Helios Book

The [Helios Book](book/index.md) provides high-level conceptual guides for the engine's major systems.

| Section | Description |
|---------|-------------|
| [Architecture](book/architecture.md) | Module layout, dependency graph, plugin system |
| [ECS](book/ecs.md) | World, entities, components, systems, scheduling |
| [Assets](book/assets.md) | Async loading, binary format, thread pool |
| [Scenes](book/scenes.md) | Serialization, SceneRoot, sub-scenes, runtime switching |
| [Rendering](book/rendering.md) | RHI, Vulkan backend, Forward+ pipeline |
| [Physics](book/physics.md) | Jolt integration, body lifecycle, contact events |
| [Audio](book/audio.md) | SoLoud, 3D spatial playback |
| [Scripting](book/scripting.md) | C# via CoreCLR, ScriptBehaviour, native bridge |
| [Editor](book/editor.md) | Panels, Play/Stop, gizmos, project system |

## API Reference

The [API Reference](api/index.md) provides detailed documentation for every major class and function in the engine.

- [Core API](api/core/ecs/world.md) (ECS, Assets, Window, Input, Scene, App)
- [Renderer API](api/renderer/index.md) (RHI, Render Graph, Pipeline)
- [Physics API](api/physics/index.md) (Jolt integration)
- [Audio API](api/audio/index.md) (SoLoud integration)
- [Scripting API](api/scripting/index.md) (C# and C++ hosting)

## Examples

Check out the [Code Snippets](examples/snippets.md) for quick "How-to" guides on common tasks.

## Getting Started

```bash
git clone git@github.com:RyuTribal/helios.git
cd helios
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --target helios-editor
./bin/helios-editor /path/to/project.hveproject
```

## Source

[GitHub Repository](https://github.com/RyuTribal/helios)
