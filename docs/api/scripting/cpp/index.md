# C++ Scripting API Reference

The Helios C++ scripting API provides the foundational layer for hosting the .NET CoreCLR runtime and integrating C# script execution into the engine's ECS-based architecture.

## Overview

The scripting system is divided into three main responsibilities at the C++ level:

- **Hosting & Runtime:** Managing the .NET CoreCLR lifecycle, assembly loading, and cross-boundary method invocation.
- **Plugin Integration:** Hooking the scripting systems into the Helios `App` and managing global state.
- **ECS Integration:** Providing components and resources that link entities to their managed script counterparts.

## Key Modules

### [ScriptingPlugin](scripting_plugin.md)
The primary entry point for enabling C# scripting. It handles configuration and registers all necessary ECS systems for script updates, collision dispatch, and hot reloading.

### [ScriptRuntime](script_runtime.md)
An abstraction over the managed runtime. It provides low-level methods for loading assemblies and invoking managed code. The default implementation is `CoreCLRRuntime`.

### [Components & Resources](components.md)
Defines the `ScriptInstance` component, which is attached to entities to give them scripting behavior, along with helper resources like `ScriptExecutionState`.

## Related Pages

- [C# Scripting API](../csharp/index.md)
- [Scripting Guide](../../../book/scripting.md)
- [ECS Overview](../../../book/ecs.md)
