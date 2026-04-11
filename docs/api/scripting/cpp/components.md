# Scripting Components

The scripting system uses several ECS components and resources to manage the lifecycle of managed script instances.

## Components

### `ScriptInstance`

The primary component for adding a script to an entity.

```cpp
struct ScriptInstance {
    std::string script_class_name; // Fully-qualified C# class name
    uint32_t script_type_id = 0;   // Numeric ID for batching (managed internally)
    uint64_t managed_handle = 0;   // GCHandle to the C# object (managed internally)
};
```

### `ScriptInitialized`

A marker component added by `script_create_system` after the managed instance has been created. It prevents the system from attempting to re-instantiate the script on subsequent frames.

## Resources

### `ScriptExecutionState`

A resource used to globally enable or disable script execution.

```cpp
struct ScriptExecutionState {
    bool running = false;
};
```

### `ScriptAliveTracker`

Used internally by `script_destroy_system` to track which script entities were alive in the previous frame, allowing for the detection of despawned entities and calling `OnDestroy` in C#.

## Related Pages

- [ScriptingPlugin](scripting_plugin.md)
- [ScriptRuntime](script_runtime.md)
- [Entity C# API](../csharp/entity.md)

