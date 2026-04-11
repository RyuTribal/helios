# Assets

## AssetServer

The `AssetServer` manages loading, caching, and lifetime of all asset data.
It is stored as a world resource (`std::shared_ptr<AssetServer>`) and shared
across the engine.

```cpp
auto& server = world.resource<std::shared_ptr<AssetServer>>();
```

### Construction

```cpp
AssetServer server(asset_root_path, thread_pool);
```

The `asset_root` is the base directory. All load paths are relative to it.
The optional `ThreadPool` pointer enables async loading on background
threads.

### Registering importers

Each asset type needs an importer registered before it can be loaded:

```cpp
server.register_importer<MeshAsset>([](const std::filesystem::path& path,
                                        AssetServer& server) -> std::any {
    // parse file, return MeshAsset
});

server.register_extensions<MeshAsset>({"gltf", "glb", "hvemesh"});
```

The built-in `AssetPlugin` registers importers for meshes, textures, and
audio automatically.

### Loading

**Async load** (returns immediately, loading happens on a background thread):

```cpp
Handle<MeshAsset> mesh = server->load<MeshAsset>("Meshes/Sphere.hvemesh");
// mesh may not be ready yet -- poll or use it; the renderer handles null data
```

**Sync load** (blocks until complete):

```cpp
Handle<MeshAsset> mesh = server->load_sync<MeshAsset>("Meshes/Sphere.hvemesh");
// mesh is ready to use immediately (or null on failure)
```

**Load by extension** (type inferred from file extension):

```cpp
AssetHandle raw = server->load_by_extension("Textures/wood.hvetex");
```

### Caching

The server deduplicates loads by `(type, path)`. Requesting the same asset
twice returns the same handle. The handle's internal refcount increments on
each copy.

### Status

```cpp
AssetStatus status = server->status(handle.untyped());
bool ready = server->is_loaded(handle.untyped());
```

`AssetStatus` is `Loading`, `Loaded`, or `Failed`.

### Resolving

```cpp
const MeshAsset* data = server->get<MeshAsset>(handle.untyped());
if (data) {
    // use vertex/index data
}
```

## Handle\<T\>

`Handle<T>` is an RAII refcounted wrapper around a raw `AssetHandle`.

- Construction acquires a refcount on the `AssetServer`.
- Copy increments the refcount.
- Move transfers ownership (no refcount change).
- Destruction decrements the refcount.

When refcount reaches zero, the asset becomes eligible for garbage
collection.

```cpp
Handle<MeshAsset> a = server->load<MeshAsset>("mesh.hvemesh");
Handle<MeshAsset> b = a;   // refcount = 2
a.reset();                  // refcount = 1
// b goes out of scope -> refcount = 0 -> eligible for GC
```

### Null handles

A default-constructed `Handle<T>` is null. Acquire/release are no-ops on
null handles. Check with `if (handle) { ... }`.

### Untyped access

```cpp
AssetHandle raw = handle.untyped();  // for maps, serialization, etc.
```

### From raw (no refcount)

```cpp
Handle<MeshAsset> h = Handle<MeshAsset>::from(raw_handle);
```

Useful for deserialization and tests where the server is not available.

## Batch Loading

For loading screens and level transitions, batch multiple loads and track
progress:

```cpp
auto batch = server->load_batch()
    .add<MeshAsset>("Meshes/Hero.hvemesh")
    .add<TextureAsset>("Textures/Hero_Albedo.hvetex")
    .add<TextureAsset>("Textures/Hero_Normal.hvetex")
    .submit();

// Each frame
float pct = batch.progress();         // 0.0 to 1.0
bool done = batch.is_complete();
auto fails = batch.failed();          // paths that errored
```

## Dependency Tracking

Importers can declare sub-asset dependencies. When a parent asset's refcount
reaches zero, all children are automatically released:

```cpp
server->add_dependency(mesh_handle, texture_handle);
```

This is used internally by the mesh importer -- a loaded `MeshAsset` holds
references to its `MaterialAsset`, which in turn references `TextureAsset`s.
The entire tree is released when the mesh is no longer needed.

## Binary Asset Format

Helios uses a custom binary format for imported assets. All binary asset files
share the same header:

```
[8  bytes]  Magic: "HLASSET\0" (0x0054455353414C48)
[16 bytes]  GUID (two uint64_t, high + low)
[4  bytes]  Type (AssetBinaryType enum)
[4  bytes]  Version
[4  bytes]  Metadata entry count N
[N entries] Key-value pairs (length-prefixed strings)
[4  bytes]  Payload data size D
[D  bytes]  Payload
```

### Asset types and extensions

| Type | Extension | Content |
|---|---|---|
| Mesh | `.hvemesh` | Vertex + index data (PBRVertex layout) |
| Texture | `.hvetex` | Raw pixel data (RGBA8 or RGBA32F for HDR) |
| CubeMap | `.hvecube` | HDR environment map |
| Audio | `.hveaudio` | PCM/WAV audio data |
| Shader | `.hlshader` | SPIR-V bytecode |
| Material | `.hvemat` | PBR material (texture references + parameters) |
| Scene | `.hvescn` | YAML scene file |

### Reading and writing

```cpp
// Write
auto bytes = write_asset_binary(header, payload_data);
// bytes is ready to write to disk

// Read
auto result = read_asset_binary(bytes.data(), bytes.size());
if (result) {
    auto& [header, data_reader] = *result;
    // header.guid, header.type, header.metadata
    // data_reader provides payload access
}

// Read header only (fast GUID/type extraction)
auto hdr = read_asset_header(bytes.data(), bytes.size());
```

## Import / Export Pipeline

The `AssetServer` supports a full import/export pipeline for converting
between external formats (glTF, PNG, WAV) and Helios binaries.

### Registering importers and exporters

```cpp
server.register_asset_importer(AssetBinaryType::Mesh, import_fn, settings);
server.register_asset_exporter(AssetBinaryType::Mesh, "glb", export_fn);
```

### Importing

```cpp
std::string asset_path = server.import_asset(
    source_path,                        // e.g., "/models/hero.glb"
    AssetBinaryType::Mesh,
    "Meshes/Hero.hvemesh",              // destination relative to asset root
    extra_metadata                       // optional key-value pairs
);
```

This generates a new GUID, calls the registered importer, and writes the
Helios binary file. Metadata includes `_source_path` and `_source_hash`
for reimport tracking.

### Re-importing

```cpp
server.reimport_asset(asset_path, new_source_path);
```

Preserves the original GUID. Updates the source hash for change detection.

### Exporting

```cpp
auto bytes = server.export_asset(asset_path, "glb");
// bytes contains the exported file in the requested format
```

### Change detection

```cpp
if (server.is_source_outdated("Meshes/Hero.hvemesh")) {
    server.reimport_asset(...);
}
```

Compares the stored source hash with the current file on disk.

## Asset Types

### MeshAsset

```cpp
struct PBRVertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;
    glm::vec4 tangent;  // xyz = tangent, w = handedness
};

struct MeshAsset {
    std::vector<PBRVertex> vertices;
    std::vector<uint32_t> indices;
    Handle<MaterialAsset> default_material;
};
```

### TextureAsset

```cpp
struct TextureAsset {
    std::vector<uint8_t> pixels;  // RGBA8 or RGBA32F (HDR)
    uint32_t width, height;
    bool hdr;
};
```

### MaterialAsset

```cpp
struct MaterialAsset {
    Handle<TextureAsset> albedo;
    Handle<TextureAsset> normal;
    Handle<TextureAsset> metallic_roughness;
    Handle<TextureAsset> emissive;
    glm::vec3 base_color;
    float metallic;
    float roughness;
};
```

## ThreadPool

A single `ThreadPool` instance is shared across the engine. Created
automatically in `App::App()` and inserted as a resource:

```cpp
auto& pool = world.resource<std::shared_ptr<ThreadPool>>();
```

```cpp
auto future = pool->submit([] { /* work */ });
future.get();  // block until done

pool->wait_idle();  // block until all tasks finish
pool->thread_count();  // number of worker threads
```

Default thread count: `hardware_concurrency - 1` (minimum 1).

## Garbage Collection

Call periodically to free assets with zero refcount:

```cpp
auto freed = server->collect_garbage();
```

Or explicitly unload a single asset:

```cpp
server->unload(handle);
```
