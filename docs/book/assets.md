# Assets & Resource Management

## AssetServer

The `AssetServer` manages the loading, caching, and lifetime of all asset data. It is stored as a world resource via a `shared_ptr` to ensure it can be safely shared across systems and background loading threads.

### Accessing the AssetServer

In a system, access the `AssetServer` using the `Res` or `ResMut` system parameters:

```cpp
void my_system(Res<std::shared_ptr<AssetServer>> asset_server) {
    // Use the server
    auto mesh = (*asset_server)->load<MeshAsset>("Meshes/Hero.hlasset");
}
```

### Registration

Each asset type must have an importer registered. The `AssetPlugin` handles standard types, but you can register custom ones:

```cpp
server->register_importer<MyAsset>([](const std::filesystem::path& path, AssetServer& s) -> std::any {
    // Load and return MyAsset
    return MyAsset{};
});

server->register_extensions<MyAsset>({"myasset", "hvemy"});
```

### Loading Assets

**Async Loading** (Non-blocking):
Returns a `Handle<T>` immediately. The asset is loaded on a background thread.

```cpp
Handle<MeshAsset> mesh = server->load<MeshAsset>("Meshes/Hero.hlasset");
```

**Sync Loading** (Blocking):
Blocks the current thread until the asset is loaded.

```cpp
Handle<MeshAsset> mesh = server->load_sync<MeshAsset>("Meshes/Hero.hlasset");
```

### Status and Resolution

Check the status of an asset or resolve a handle to its underlying data:

```cpp
AssetStatus status = server->status(mesh.untyped());
if (server->is_loaded(mesh.untyped())) {
    const MeshAsset* data = server->get<MeshAsset>(mesh.untyped());
    // Use data...
}
```

## Handle\<T\>

A `Handle<T>` is a reference-counted smart pointer to an asset. 

- **Acquire/Release:** Automatically increments the refcount on construction/copy and decrements on destruction.
- **Untyped Access:** Use `handle.untyped()` to get the raw `AssetHandle` (e.g., for internal engine APIs or serialization).
- **Null Checks:** Handles can be checked for validity: `if (mesh) { ... }`.

## Binary Asset Format (.hlasset)

Helios uses a unified binary format (`.hlasset`) for all imported assets. This format is optimized for fast loading and contains a GUID-based header.

### Header Structure

All `.hlasset` files start with a fixed-size header followed by variable-length metadata and the payload:

| Offset | Size | Field | Description |
|---|---|---|---|
| 0 | 8 | Magic | `ASSET_BINARY_MAGIC` ("HLASSET\0") |
| 8 | 16 | GUID | Unique identifier (two `uint64_t`) |
| 24 | 4 | Type | `AssetBinaryType` enum |
| 28 | 4 | Version | Format version (currently 1) |
| 32 | 4 | Meta Count | Number of metadata entries |
| ... | ... | Metadata | Key-value pairs (length-prefixed strings) |
| ... | 4 | Data Size | Size of the payload |
| ... | D | Payload | Raw asset data |

### Asset Types

| Type | Default Extensions |
|---|---|
| **Mesh** | `.hlasset`, `.gltf`, `.glb` |
| **Texture** | `.hlasset`, `.png`, `.jpg`, `.hdr` |
| **Audio** | `.hlasset`, `.wav`, `.ogg` |
| **Shader** | `.hlasset`, `.spv` |
| **Scene** | `.hlasset`, `.hvescn` |

## Import & Export Pipeline

The `AssetServer` provides a pipeline for converting source files (like `.glb` or `.png`) into optimized `.hlasset` binaries.

### Importing

Importing a source file generates a new `.hlasset` file with a unique GUID:

```cpp
std::string hlasset_path = server->import_asset(
    "Source/Models/Hero.glb",
    AssetBinaryType::Mesh,
    "Meshes/Hero.hlasset"
);
```

### Re-importing

If a source file changes, you can re-import it while preserving the original GUID:

```cpp
if (server->is_source_outdated("Meshes/Hero.hlasset")) {
    server->reimport_asset("Meshes/Hero.hlasset", "Source/Models/Hero_V2.glb");
}
```

## Garbage Collection

Assets are automatically garbage-collected when their reference count reaches zero. The `AssetPlugin` adds a system to `PreUpdate` that triggers this:

```cpp
server->collect_garbage(); // Unloads all assets with refcount 0
```

You can also explicitly unload an asset:

```cpp
server->unload(mesh.untyped());
```
