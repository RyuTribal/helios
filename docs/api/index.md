# API Reference

The Helios API Reference provides detailed documentation for the engine's core modules, rendering hardware interface, and scripting bridge.

## Core Modules

The **Core** module contains the foundation of the engine, including the ECS, asset management, and windowing.

- **[ECS](core/ecs/world.md)**: World, entities, components, systems, and scheduling.
- **[Assets](core/assets/asset_server.md)**: Asynchronous loading, reference counting, and binary format.
- **[Window & Input](core/window/windows.md)**: Window management and raw input polling.
- **[Scene](core/scene/scene_manager.md)**: Scene lifecycle and YAML serialization.
- **[App](core/app/app.md)**: Application setup and main loop.

## Renderer

The **Renderer** module provides a high-performance Forward+ pipeline and a low-level RHI.

- **[Renderer Overview](renderer/index.md)**: Introduction to the rendering stack.
- **[RHI](renderer/rhi/device.md)**: GPU resource management (Device, Texture, Buffer).
- **[Render Plugin](renderer/render_plugin.md)**: Integration with the ECS.

## Physics & Audio

Independent modules for simulation and sound.

- **[Physics](physics/index.md)**: Jolt integration, rigid bodies, and raycasting.
- **[Audio](audio/index.md)**: SoLoud integration and spatial audio.

## Scripting

Cross-language support for C# and C++.

- **[Scripting Overview](scripting/index.md)**: Architecture and hosting layer.
- **[C# API](scripting/csharp/index.md)**: User-facing classes for game logic.
- **[C++ Hosting](scripting/cpp/index.md)**: CoreCLR integration and plugin.
