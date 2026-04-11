# Asset Binary (.hlasset)

The `.hlasset` file format is the internal binary format used by the Helios engine for optimized asset loading.

## File Format

All `.hlasset` files follow a structured binary layout designed for fast parsing and metadata extraction.

### Header Structure

The header is fixed-length plus variable-length metadata:

- **Magic Number** (8 bytes): `HLASSET\0` (0x0054455353414C48)
- **GUID** (16 bytes): A unique identifier for the asset.
- **Type** (4 bytes): `AssetBinaryType` (e.g., Mesh, Texture).
- **Version** (4 bytes): File format version.
- **Metadata Count** (4 bytes): Number of key-value pairs.
- **Metadata Pairs**: Length-prefixed keys and values.
- **Data Size** (4 bytes): Size of the asset payload.
- **Payload**: The raw asset data (e.g., compressed texture data, vertex buffers).

## API functions

| Function | Description |
| :--- | :--- |
| `write_asset_binary(header, data)` | Serialize a complete asset file with the given header and data. |
| `read_asset_binary(bytes, size)` | Deserialize a complete asset file, returning the header and a payload reader. |
| `read_asset_header(bytes, size)` | Quick extraction of the GUID and type without reading the full payload. |

## Asset Binary Types

The engine defines several asset categories:

- `Unknown`
- `Mesh`
- `Texture`
- `CubeMap`
- `Audio`
- `Shader`
- `Material`
- `Scene`

## Metadata

Metadata allows storing arbitrary key-value pairs alongside the asset. Examples include:
- `source_path`: The original source file path (e.g., `.png`).
- `source_hash`: The hash of the source file for detecting changes.
- `texture_format`: Specific compression format for textures.
- `mesh_scale`: Default scaling for a 3D model.
