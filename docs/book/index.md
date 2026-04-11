# The Helios Book

Welcome to **The Helios Book**, the comprehensive guide to the Helios Engine. Whether you're a seasoned engine developer or just starting your journey into high-performance graphics, this book will walk you through the architecture, systems, and workflows of Helios.

## What is Helios?

Helios is a modular, data-driven game engine built from the ground up using **C++20** and **Vulkan 1.3**. It is designed with a focus on performance, clarity, and modern engine architecture.

Key features include:
- **Data-Oriented Design:** A custom, high-performance Entity Component System (ECS) at its core.
- **Modern Graphics:** A Forward+ rendering pipeline utilizing Vulkan 1.3 features.
- **Cross-Language Scripting:** Seamless integration with C# for high-level logic.
- **Modular Architecture:** A plugin-based system that allows you to swap or extend any part of the engine.

## Learning Path

If you're new to Helios, we recommend following this path to get up to speed:

1.  **[Architecture](architecture.md):** Understand the high-level design and the plugin system.
2.  **[ECS](ecs.md):** Dive into the core data-driven model that powers everything.
3.  **[Assets](assets.md):** Learn how assets are imported, managed, and uploaded to the GPU.
4.  **[Rendering](rendering.md):** Explore the Forward+ pipeline and Vulkan integration.
5.  **[Scripting](scripting.md):** Start writing game logic in C#.

## Hello World

Setting up a basic Helios application is straightforward. Here is a minimal example that initializes the engine with a window and a basic rendering setup:

```cpp
#include <helios/ecs/ecs.h>
#include <helios/window/window_plugin.h>
#include <helios/render_plugin.h>
#include <helios/forward_plus/forward_plus_plugin.h>

using namespace helios;

int main() {
    App app;

    // 1. Add Windowing support
    app.add_plugin(WindowPlugin{ .primary_window = WindowDesc{
        .title = "Hello Helios",
        .width = 1280,
        .height = 720,
    }});

    // 2. Add Rendering core
    app.add_plugin(RenderPlugin{});

    // 3. Add Forward+ Pipeline
    app.add_plugin(ForwardPlusPlugin{});

    // 4. Run the engine
    app.run();

    return 0;
}
```

This snippet initializes a window, sets up the Vulkan renderer, and starts the main engine loop. From here, you can start spawning entities, adding components, and building your world.
