# ScriptRuntime

The `ScriptRuntime` is an abstract interface for a managed scripting runtime. The production implementation is `CoreCLRRuntime`, which is backed by .NET CoreCLR via HostFXR.

## Interface

### Assembly Management

| Method | Description |
| :--- | :--- |
| `bool load_assembly(const std::filesystem::path& path)` | Load a game assembly (.dll) from disk. |
| `void unload_assembly()` | Unload the currently loaded game assembly. |
| `bool reload_assembly(const std::filesystem::path& path)` | Reload the game assembly. Internally: unload -> load. |

### Class Discovery

| Method | Description |
| :--- | :--- |
| `bool class_exists(const std::string& name) const` | Returns true if the given fully-qualified class name exists. |
| `std::vector<std::string> get_script_class_names() const` | Returns all `ScriptBehaviour` subclass names found in the loaded assembly. |
| `uint32_t get_script_type_id(const std::string& name) const` | Returns a numeric type ID for a script class name. |

### Instance Lifecycle

| Method | Description |
| :--- | :--- |
| `uint64_t invoke_create(const std::string& class_name, uint64_t entity_id)` | Create a managed script instance for the given entity. Returns a managed handle. |
| `void invoke_update(uint32_t script_type_id, ...)` | Call `OnUpdate` on a batch of entities sharing the same script type. |
| `void invoke_destroy(uint64_t entity_id, uint64_t managed_handle)` | Destroy the managed instance for a single entity. |
| `void destroy_all_instances()` | Destroy all managed instances (e.g., on scene unload). |
| `void invoke_on_collision(...)` | Invoke `OnCollisionEnter` on the script instance. |

## Usage

The `ScriptRuntime` is typically accessed as a world resource:

```cpp
auto* runtime_ptr = world.try_resource<std::unique_ptr<ScriptRuntime>>();
if (runtime_ptr && *runtime_ptr) {
    ScriptRuntime* rt = runtime_ptr->get();
    rt->request_reload();
}
```

## Related Pages

- [ScriptingPlugin](scripting_plugin.md)
- [Components & Resources](components.md)
- [C# Scripting Overview](../csharp/index.md)
