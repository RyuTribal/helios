# Helios Engine Documentation Implementation Plan

> **For Gemini:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Create an in-depth, Bevy-inspired documentation suite for the Helios game engine, featuring a conceptual "Book" and a comprehensive, file-by-file "API Reference" with usable examples and cross-links.

**Architecture:** A hybrid approach using MkDocs (as indicated by the existing `mkdocs.yml`). The documentation will be split into a conceptual guide (The Helios Book) and a detailed technical reference (API Reference), organized by module (`core`, `renderer`, `physics`, `audio`, `script`, `editor`).

**Tech Stack:** Markdown, MkDocs, C++ (for reading source), C# (for scripting docs).

---

## Phase 1: The Helios Book (Core Concepts & Guides)

### Task 1: Book Structure Setup
**Files:**
- Modify: `mkdocs.yml`
- Create: `docs/book/index.md`

**Step 1: Update MkDocs Navigation**
Modify `mkdocs.yml` to structure the navigation into "The Book" and "API Reference". Ensure the theme and plugins (like `search` and `codehilite`) are configured for extensive code examples.

**Step 2: Create Book Index**
Create `docs/book/index.md` as the landing page for the conceptual guide. It should introduce the engine's philosophy (modular, data-driven, C++20, Vulkan) and provide a learning path.

### Task 2: Architecture & App Lifecycle Guide
**Files:**
- Modify: `docs/book/architecture.md` (move from `docs/architecture.md` and expand)
- Read: `helios-core/src/helios/app/app.h`, `helios-core/src/helios/app/game_flow.h`

**Step 1: Expand Architecture Doc**
Detail the module layout, dependency graph, and the `Plugin` system. Add clear code examples of creating an `App`, adding plugins, and the `Schedule` execution order (Startup, Update, FixedUpdate, etc.). Explain the main loop and `FixedTimeAccumulator`.

### Task 3: ECS Deep Dive
**Files:**
- Modify: `docs/book/ecs.md` (move from `docs/ecs.md` and expand)
- Read: `helios-core/src/helios/ecs/world.h`, `helios-core/src/helios/ecs/query.h`, `helios-core/src/helios/ecs/commands.h`, `helios-core/src/helios/ecs/scheduler.h`

**Step 1: Document World, Entities, and Components**
Explain the Archetype-based ECS. Provide examples of defining components, spawning entities, and adding/removing components.

**Step 2: Document Queries and Systems**
Provide comprehensive examples of `Query`, `Res`, `ResMut`, `Commands`, and system parameters. Explain how to write systems and register them in the `Schedule`. Detail parallel execution.

### Task 4: Assets & Resource Management
**Files:**
- Modify: `docs/book/assets.md`
- Read: `helios-core/src/helios/assets/asset_server.h`, `helios-core/src/helios/assets/asset.h`

**Step 1: Document Asset Loading**
Explain the async loading pipeline, the `AssetServer`, and `Handle<T>`. Provide examples of loading a mesh, texture, or audio file, and how to check load status. Detail the binary format and custom importers.

### Task 5: Rendering Pipeline Guide
**Files:**
- Modify: `docs/book/rendering.md`
- Read: `helios-renderer/src/helios/render_plugin.h`, `helios-renderer/src/helios/rhi/rhi_types.h`

**Step 1: Document the Renderer**
Explain the Forward+ pipeline, `RenderContext`, `RenderSettings`, and the camera setup (`camera_driver`). Provide examples of setting up a scene for rendering (lights, meshes, materials).

### Task 6: Scripting Integration Guide
**Files:**
- Modify: `docs/book/scripting.md`
- Read: `helios-script/src/scripting_plugin.h`, `ScriptCore/Source/Helios/Entity.cs`

**Step 1: Document C# Scripting**
Explain the CoreCLR integration, `ScriptBehaviour`, and the native bridge. Provide complete examples of creating a C# script, attaching it to an entity, and interacting with core engine components (e.g., `Transform`). Detail the hot-reload workflow.

### Task 7: Physics, Audio, and Scenes Guides
**Files:**
- Modify: `docs/book/physics.md`, `docs/book/audio.md`, `docs/book/scenes.md`

**Step 1: Document Auxiliary Systems**
Expand the remaining guides with practical examples.
- Physics: Jolt integration, rigid bodies, colliders, contact events.
- Audio: SoLoud integration, playing spatial sounds.
- Scenes: Serialization, `SceneRoot`, runtime switching.

---

## Phase 2: API Reference (Module-by-Module Source Documentation)

*Note: This phase systematically documents the public headers of each module.*

### Task 8: `helios-core` API Reference (ECS & App)
**Files:**
- Create: `docs/api/core/ecs/world.md`, `docs/api/core/ecs/query.md`, `docs/api/core/ecs/commands.md`, `docs/api/core/app/app.md`, etc.
- Modify: `mkdocs.yml` (add new pages)

**Step 1: Document ECS Headers**
Create reference pages for every major class in `helios-core/src/helios/ecs` and `helios-core/src/helios/app`.
- List class members, methods, and their purposes.
- Provide a small, isolated code snippet for using the class.
- Add links to related parts (e.g., `Commands` -> `World`).

### Task 9: `helios-core` API Reference (Assets, Window, Input, Scene)
**Files:**
- Create: `docs/api/core/assets/asset_server.md`, `docs/api/core/input/input.md`, `docs/api/core/window/window.md`, etc.

**Step 1: Document Core Subsystems**
Create reference pages for the asset management, window abstraction, input polling (`Input` resource, mouse/keyboard state), and scene graph/serialization utilities.

### Task 10: `helios-renderer` API Reference
**Files:**
- Create: `docs/api/renderer/rhi/device.md`, `docs/api/renderer/rhi/command_buffer.md`, `docs/api/renderer/render_plugin.md`, `docs/api/renderer/camera.md`, etc.

**Step 1: Document RHI and Render Graph**
Document the Vulkan abstraction layer (`rhi::Device`, `Texture`, `Buffer`, `Pipeline`). Detail the `RenderPlugin` and camera structures.

### Task 11: `helios-physics` & `helios-audio` API Reference
**Files:**
- Create: `docs/api/physics/physics_plugin.md`, `docs/api/physics/components.md`, `docs/api/audio/audio_plugin.md`, `docs/api/audio/soloud_device.md`, etc.

**Step 1: Document Physics and Audio Interfaces**
Detail the physics components (colliders, rigid bodies), world interface, and audio device management.

### Task 12: `helios-script` & `ScriptCore` API Reference
**Files:**
- Create: `docs/api/scripting/native_bridge.md`, `docs/api/scripting/csharp/entity.md`, `docs/api/scripting/csharp/components.md`, etc.

**Step 1: Document Scripting APIs**
Document the C++ side (`script_execution_system`, `coreclr_runtime`) and the C# API provided to end-users (the `Helios` namespace in `ScriptCore`). Include examples of writing scripts.

---

## Phase 3: Examples and Final Polish

### Task 13: Snippets Collection
**Files:**
- Create: `docs/examples/snippets.md`

**Step 1: Compile Common Code Snippets**
Create a central repository of common tasks:
- "How to rotate an entity every frame"
- "How to load and play a sound"
- "How to cast a ray"
- "How to query entities with specific components"

### Task 14: Cross-linking and Validation
**Files:**
- Modify: Various `.md` files in `docs/`

**Step 1: Audit and Link**
Review all documentation generated. Ensure that every mention of a core concept (e.g., `Entity`, `System`, `AssetServer`) links to its corresponding API Reference page. Ensure code blocks are correctly formatted and conceptually sound against the current codebase.