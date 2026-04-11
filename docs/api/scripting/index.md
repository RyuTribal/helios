# Scripting API Reference

The Helios scripting system is built on .NET CoreCLR, providing a high-performance C# scripting environment with a C++ hosting layer.

## Architecture Overview

The scripting system is divided into two main parts:

1.  **C++ Hosting Layer (`helios-script`)**: Responsible for booting the .NET runtime, loading assemblies, and bridging engine events (Update, Physics, Input) to managed code.
2.  **C# ScriptCore (`ScriptCore`)**: A managed library that provides the base classes and API for user scripts, including entity/component access, physics, and scene management.

## API Navigation

### C++ API (Hosting & ECS)

- [**ScriptingPlugin**](cpp/scripting_plugin.md): The entry point for integrating scripting into an `App`.
- [**ScriptRuntime**](cpp/script_runtime.md): The low-level interface for assembly and instance management.
- [**Components**](cpp/components.md): ECS components used to attach scripts to entities.

### C# API (User Scripts)

- [**ScriptBehaviour**](csharp/script_behaviour.md): The base class for all user scripts.
- [**Entity**](csharp/entity.md): The primary wrapper for interacting with world objects.
- [**TransformComponent**](csharp/transform_component.md): API for position, rotation, and scale.
- [**Input**](csharp/input.md): Static helper for keyboard and mouse state.
- [**Audio**](csharp/audio.md): Static helper for playing sounds.
- [**Scene**](csharp/scene.md): Static helper for runtime scene management.

## Related Pages

- [Scripting Overview (Book)](../../book/scripting.md)
- [ECS Overview (Book)](../../book/ecs.md)
- [C++ API Reference](cpp/index.md)
- [C# API Reference](csharp/index.md)

