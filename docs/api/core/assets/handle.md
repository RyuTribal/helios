# Handle<T>

`Handle<T>` is a smart pointer wrapper that tracks the usage of an asset. It provides reference-counted access to an asset managed by the `AssetServer`.

## Overview

When a `Handle<T>` is created, it increments the reference count of the corresponding asset. When the handle is destroyed, it decrements the reference count. If an asset's reference count reaches zero, it becomes eligible for garbage collection.

### Characteristics
- **RAII**: Memory-safe resource management.
- **Move-only and Copyable**: Handles can be passed between systems and stored in components.
- **Type-Safe**: Specifically typed for an asset class (e.g., `Handle<MeshAsset>`).

## Methods

| Method | Description |
| :--- | :--- |
| `Handle()` | Create a null handle. |
| `reset()` | Explicitly release the reference and become a null handle. |
| `untyped()` | Convert the typed handle into a type-erased `AssetHandle`. |
| `index()` | Get the underlying asset index. |
| `generation()` | Get the underlying asset generation. |
| `operator bool()` | Check if the handle is not null. |

## Usage Example

```cpp
// Storing a handle in a component
struct MeshRenderer {
    Handle<MeshAsset> mesh;
    Handle<MaterialAsset> material;
};

// Creating a handle manually (not common)
Handle<MeshAsset> handle(raw_handle, &asset_server);
```

### Reference Management

The `Handle<T>` interacts with the `AssetServer` to manage the life cycle of assets. It is the primary way systems interact with the asset system.

```cpp
// Copying increments the reference count
Handle<TextureAsset> h1 = server.load<TextureAsset>("t.png");
Handle<TextureAsset> h2 = h1; // refcount = 2

// Re-assigning handles handles release and acquire correctly
h1 = server.load<TextureAsset>("other.png"); // release "t.png", acquire "other.png"
```

## Related Pages
- [AssetServer](asset_server.md)
