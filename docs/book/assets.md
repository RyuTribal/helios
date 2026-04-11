# Assets & Resource Management

## AssetServer

The **[`AssetServer`](../api/core/assets/asset_server.md)** manages the loading, caching, and lifetime of all asset data. It is stored as a world resource via a `shared_ptr` to ensure it can be safely shared across systems and background loading threads.

### Accessing the AssetServer

In a system, access the `AssetServer` using the `Res` or `ResMut` system parameters:

```cpp
void my_system(Res<std::shared_ptr<AssetServer>> asset_server) {
    // Use the server
    auto mesh = (*asset_server)->load<MeshAsset>("Meshes/Hero.hlasset");
}
```

### Custom Importers

Each asset type must have an importer registered. The `AssetPlugin` handles standard types, but you can register custom ones:

```cpp
server->register_importer<MyAsset>([](const std::filesystem::path& path, AssetServer& s) -> std::any {
    // Load and return MyAsset
    return MyAsset{};
});

server->register_extensions<MyAsset>({"myasset", "hvemy"});
```

#### Sub-asset Creation

Complex assets, like a GLB model, often contain multiple internal sub-assets (e.g., embedded textures, materials). To manage these properly, importers can use `server.store()` and `server.add_dependency()`.

- **`server.store(path, asset)`**: Manually registers an asset that doesn't have its own file. The `path` is used as a unique cache key (often the parent's path + a name). Returns a raw `AssetHandle`.
- **`server.add_dependency(parent, child)`**: Tells the server that the `child` asset's lifetime is tied to the `parent`. When the parent's refcount reaches zero, it will also release its reference to the child.

**Example from a GLB Importer:**

```cpp
// 1. Create sub-asset (e.g., embedded texture)
TextureAsset tex = load_from_buffer(glb_data);

// 2. Store it manually in the server
AssetHandle tex_handle = server.store("Models/Hero.glb::Texture_0", std::move(tex));

// 3. Link child's lifetime to the parent mesh
server.add_dependency(mesh_handle, tex_handle);
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

### Diverse Loading Examples

The `AssetServer` can load any registered type. Common examples:

```cpp
// Textures (PNG, JPG, HDR)
Handle<TextureAsset> albedo = server->load<TextureAsset>("Textures/Grass_Albedo.png");

// Audio (WAV, OGG, MP3)
Handle<AudioData> music = server->load<AudioData>("Music/MainTheme.ogg");
```

### Loading Batches

Use a `LoadBatch` to track the loading status of multiple assets. This is ideal for loading a scene or a set of resources for a specific entity.

```cpp
// 1. Start a batch
auto builder = server->load_batch();

// 2. Add multiple assets (all types supported)
builder.add<MeshAsset>("Meshes/Hero.hlasset")
       .add<TextureAsset>("Textures/Hero_Base.png")
       .add<AudioData>("Sfx/Hero_Spawn.wav");

// 3. Submit and get a tracker
LoadBatch batch = builder.submit();

// 4. Check progress or completion
if (batch.is_complete()) {
    float percent = batch.progress() * 100.0f;
    int remaining = batch.remaining();
}
```

- **Reference Counting:** The `LoadBatch` holds a strong reference to all assets within it. When the batch is destroyed, those references are released unless you've stored the handles elsewhere.
- **Progress Tracking:** `batch.progress()` returns a `float` (0.0 to 1.0) indicating the percentage of assets that have either finished loading or failed.

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

A **[`Handle<T>`](../api/core/assets/handle.md)** is a reference-counted smart pointer to an asset. Under the hood, it wraps a raw `AssetHandle`.

### AssetHandle

The **[`AssetHandle`](../api/core/assets/handle.md)** is a 64-bit packed ID consisting of:
- **32-bit Index:** Points to the asset's slot in the internal storage array.
- **32-bit Generation:** Incremented each time a slot is reused, preventing "dangling" handles to destroyed assets.

Because it is a simple 64-bit POD (Plain Old Data) type, it is extremely efficient to copy, pass, and store. It contains no pointers, making it safe for serialization and use in multi-threaded contexts.

- **Acquire/Release:** `Handle<T>` automatically increments the refcount on construction/copy and decrements on destruction.
- **Untyped Access:** Use `handle.untyped()` to get the raw `AssetHandle` (e.g., for internal engine APIs or serialization).
- **Null Checks:** Handles can be checked for validity: `if (mesh) { ... }`.

## Binary Asset Format (.hlasset)

Helios uses a unified **[binary format](../api/core/assets/asset_binary.md)** (`.hlasset`) for all imported assets. This format is optimized for fast loading and contains a GUID-based header.

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
