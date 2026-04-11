# AssetServer

The `AssetServer` is the central hub for loading, managing, and hot-reloading game assets. It handles asynchronous loading, reference counting, and automatic garbage collection.

## Overview

The `AssetServer` tracks all loaded assets by their file paths and ensures that each asset is only loaded once. It uses a thread pool for background loading and provides a `Handle<T>` RAII wrapper for safe resource management.

### Key Features
- **Async Loading**: Non-blocking asset loading on background threads.
- **Reference Counting**: Automatic tracking of asset usage via `Handle<T>`.
- **Garbage Collection**: Unloads assets that are no longer referenced.
- **Dependency Tracking**: Automatically keeps sub-assets (like textures in a material) alive.
- **Hot Reloading**: Watches for file changes and reloads assets at runtime.

## Methods

### Loading

| Method | Description |
| :--- | :--- |
| `load<T>(path)` | Asynchronously load an asset of type `T`. Returns a `Handle<T>` immediately. |
| `load_sync<T>(path)` | Synchronously load an asset of type `T`. Blocks until finished. |
| `load_batch()` | Start a batch load operation for multiple assets with progress tracking. |
| `load_by_extension(path)` | Load an asset by inferring its type from the file extension. |

### Resolution & Status

| Method | Description |
| :--- | :--- |
| `get<T>(handle)` | Resolve a handle to a pointer to the loaded asset. Returns `nullptr` if not ready. |
| `status(handle)` | Get the current status of an asset (`Loading`, `Loaded`, `Failed`, etc.). |
| `is_loaded(handle)` | Returns `true` if the asset is fully loaded and ready for use. |
| `get_path(handle)` | Returns the file path associated with the given handle. |

### Management

| Method | Description |
| :--- | :--- |
| `register_importer<T>(fn)` | Register a function to handle loading files of type `T`. |
| `register_extensions<T>({exts})` | Map file extensions (e.g., `.png`, `.jpg`) to an asset type. |
| `collect_garbage()` | Manually trigger an unload of all assets with a zero reference count. |
| `add_dependency(parent, child)` | Ensure `child` stays alive as long as `parent` is alive. |

## Usage Example

```cpp
// Request an asset load (async)
Handle<TextureAsset> texture = asset_server.load<TextureAsset>("textures/bricks.png");

// In a system, check if it's ready before using
if (asset_server.is_loaded(texture)) {
    const TextureAsset* data = asset_server.get(texture);
    // Use texture data...
}
```

## Binary Pipeline

The `AssetServer` also manages the conversion of source files (e.g., `.glb`, `.png`) into optimized `.hlasset` binary files.

- `import_asset()`: Converts a source file to a Helios binary.
- `reimport_asset()`: Updates a binary asset from a changed source.
- `is_source_outdated()`: Checks if the source file has changed since the last import.

## Related Pages
- [Handle<T>](handle.md)
- [Asset Binary](asset_binary.md)
