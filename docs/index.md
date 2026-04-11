# Helios Engine Documentation

**v0.5.0** — A modular, data-driven game engine built with C++20 and Vulkan 1.3.

## Quick Links

| Section | Description |
|---------|-------------|
| [Architecture](architecture.md) | Module layout, dependency graph, plugin system |
| [ECS](ecs.md) | World, entities, components, systems, scheduling |
| [Assets](assets.md) | Async loading, binary format, thread pool |
| [Scenes](scenes.md) | Serialization, SceneRoot, sub-scenes, runtime switching |
| [Rendering](rendering.md) | RHI, Vulkan backend, Forward+ pipeline |
| [Physics](physics.md) | Jolt integration, body lifecycle, contact events |
| [Audio](audio.md) | SoLoud, 3D spatial playback |
| [Scripting](scripting.md) | C# via CoreCLR, ScriptBehaviour, native bridge |
| [Editor](editor.md) | Panels, Play/Stop, gizmos, project system |

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
