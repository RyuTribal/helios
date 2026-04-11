#include "helios/assets/asset_binary.h"

namespace helios {

std::vector<uint8_t> write_asset_binary(const AssetBinaryHeader& header,
                                        const std::vector<uint8_t>& data) {
    BinaryWriter w;

    // --- Fixed header ---
    w.write<uint64_t>(ASSET_BINARY_MAGIC);
    w.write<uint64_t>(header.guid.high);
    w.write<uint64_t>(header.guid.low);
    w.write<uint32_t>(static_cast<uint32_t>(header.type));
    w.write<uint32_t>(header.version);

    // --- Metadata ---
    w.write<uint32_t>(static_cast<uint32_t>(header.metadata.size()));
    for (const auto& [key, value] : header.metadata) {
        w.write_string(key);
        w.write_string(value);
    }

    // --- Data ---
    w.write<uint32_t>(static_cast<uint32_t>(data.size()));
    w.write_bytes(data);

    return w.take();
}

// Internal helper: parse the header fields from a reader.
static AssetBinaryHeader parse_header(BinaryReader& r) {
    AssetBinaryHeader header;

    header.guid.high = r.read<uint64_t>();
    header.guid.low  = r.read<uint64_t>();
    header.type      = static_cast<AssetBinaryType>(r.read<uint32_t>());
    header.version   = r.read<uint32_t>();

    uint32_t meta_count = r.read<uint32_t>();
    for (uint32_t i = 0; i < meta_count; ++i) {
        auto key   = r.read_string();
        auto value = r.read_string();
        header.metadata.emplace(std::move(key), std::move(value));
    }

    return header;
}

std::optional<AssetBinaryReadResult> read_asset_binary(const uint8_t* bytes,
                                                       size_t size) {
    if (size < sizeof(uint64_t)) return std::nullopt;

    BinaryReader r(bytes, size);

    // Validate magic
    uint64_t magic = r.read<uint64_t>();
    if (magic != ASSET_BINARY_MAGIC) return std::nullopt;

    AssetBinaryHeader header = parse_header(r);

    // Read data size and build a sub-reader over the payload region.
    uint32_t data_size = r.read<uint32_t>();
    size_t data_offset = r.offset();

    if (data_offset + data_size > size) {
        return std::nullopt;
    }

    BinaryReader data_reader(bytes + data_offset, data_size);
    return AssetBinaryReadResult{std::move(header), std::move(data_reader)};
}

std::optional<AssetBinaryHeader> read_asset_header(const uint8_t* bytes,
                                                   size_t size) {
    if (size < sizeof(uint64_t)) return std::nullopt;

    BinaryReader r(bytes, size);

    uint64_t magic = r.read<uint64_t>();
    if (magic != ASSET_BINARY_MAGIC) return std::nullopt;

    return parse_header(r);
}

} // namespace helios
