#pragma once

#include "helios/assets/binary_reader.h"
#include "helios/assets/binary_writer.h"
#include "helios/core/uuid.h"

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace helios {

/// Magic number stored at the start of every .hlasset file.
/// ASCII: "HLASSET\0" read as a little-endian uint64_t.
inline constexpr uint64_t ASSET_BINARY_MAGIC = 0x0054455353414C48ULL;

/// Identifies the high-level category of the asset payload.
enum class AssetBinaryType : uint32_t {
    Unknown  = 0,
    Mesh     = 1,
    Texture  = 2,
    CubeMap  = 3,
    Audio    = 4,
    Shader   = 5,
    Material = 6,
    Scene    = 7,
    Custom   = 1000,
};

/// Arbitrary key-value pairs attached to the asset header.
using AssetMetadata = std::unordered_map<std::string, std::string>;

/// Fixed + variable-length header written at the front of a binary asset.
///
/// Binary layout (all multi-byte fields are little-endian on LE hosts):
///
///   [8  bytes] Magic (ASSET_BINARY_MAGIC)
///   [16 bytes] GUID (high, low — two uint64_t)
///   [4  bytes] Type (AssetBinaryType)
///   [4  bytes] Version
///   [4  bytes] Metadata entry count N
///   [N x (length-prefixed key + length-prefixed value)]
///   [4  bytes] Data size D
///   [D  bytes] Payload data
struct AssetBinaryHeader {
    UUID          guid    = UUID::NIL;
    AssetBinaryType type  = AssetBinaryType::Unknown;
    uint32_t      version = 1;
    AssetMetadata metadata;
};

/// Result of reading a complete binary asset.
struct AssetBinaryReadResult {
    AssetBinaryHeader header;
    /// Reader positioned at the start of the payload data section.
    /// The reader's remaining bytes == data size recorded in the file.
    BinaryReader data;
};

/// Serialize a complete binary asset file.
///
/// @param header   The header (GUID, type, version, metadata).
/// @param data     Raw payload bytes.
/// @return         Complete file contents ready for writing to disk.
std::vector<uint8_t> write_asset_binary(const AssetBinaryHeader& header,
                                        const std::vector<uint8_t>& data);

/// Deserialize a complete binary asset file.
///
/// @param bytes    Pointer to the file contents.
/// @param size     Size of the file in bytes.
/// @return         Header + a BinaryReader positioned at the data section,
///                 or std::nullopt if the magic number does not match.
std::optional<AssetBinaryReadResult> read_asset_binary(const uint8_t* bytes,
                                                       size_t size);

/// Read only the header (skip over the data payload).
///
/// Useful for quick GUID / type extraction without reading the full payload.
///
/// @param bytes    Pointer to the file contents.
/// @param size     Size of the file in bytes.
/// @return         Parsed header, or std::nullopt on magic mismatch.
std::optional<AssetBinaryHeader> read_asset_header(const uint8_t* bytes,
                                                   size_t size);

} // namespace helios
