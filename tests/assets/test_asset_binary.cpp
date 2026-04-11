#include <gtest/gtest.h>

#include "helios/assets/asset_binary.h"
#include "helios/core/uuid.h"

#include <cstdint>
#include <string>
#include <vector>

namespace helios::test {

// ---- write_asset_binary / read_asset_binary round-trip ----

TEST(AssetBinary, RoundTripEmptyPayload) {
    AssetBinaryHeader header;
    header.guid    = UUID::generate();
    header.type    = AssetBinaryType::Mesh;
    header.version = 2;

    std::vector<uint8_t> data; // empty payload

    auto bytes = write_asset_binary(header, data);
    EXPECT_GT(bytes.size(), 0u);

    auto result = read_asset_binary(bytes.data(), bytes.size());
    ASSERT_TRUE(result.has_value());

    EXPECT_EQ(result->header.guid, header.guid);
    EXPECT_EQ(result->header.type, AssetBinaryType::Mesh);
    EXPECT_EQ(result->header.version, 2u);
    EXPECT_TRUE(result->header.metadata.empty());
    EXPECT_EQ(result->data.remaining(), 0u);
}

TEST(AssetBinary, RoundTripWithPayload) {
    AssetBinaryHeader header;
    header.guid    = UUID::generate();
    header.type    = AssetBinaryType::Texture;
    header.version = 1;

    std::vector<uint8_t> payload = {0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE};

    auto bytes = write_asset_binary(header, payload);
    auto result = read_asset_binary(bytes.data(), bytes.size());
    ASSERT_TRUE(result.has_value());

    EXPECT_EQ(result->header.guid, header.guid);
    EXPECT_EQ(result->header.type, AssetBinaryType::Texture);
    EXPECT_EQ(result->data.remaining(), payload.size());

    // Read back the payload data
    uint8_t buf[6];
    result->data.read_bytes(buf, 6);
    for (size_t i = 0; i < 6; i++) {
        EXPECT_EQ(buf[i], payload[i]);
    }
}

TEST(AssetBinary, RoundTripWithMetadata) {
    AssetBinaryHeader header;
    header.guid    = UUID::generate();
    header.type    = AssetBinaryType::Audio;
    header.version = 3;
    header.metadata["_source_path"] = "/assets/sounds/boom.wav";
    header.metadata["_source_hash"] = "abc123";
    header.metadata["artist"]       = "Test Artist";

    std::vector<uint8_t> payload = {1, 2, 3, 4, 5};

    auto bytes = write_asset_binary(header, payload);
    auto result = read_asset_binary(bytes.data(), bytes.size());
    ASSERT_TRUE(result.has_value());

    EXPECT_EQ(result->header.guid, header.guid);
    EXPECT_EQ(result->header.type, AssetBinaryType::Audio);
    EXPECT_EQ(result->header.version, 3u);
    EXPECT_EQ(result->header.metadata.size(), 3u);
    EXPECT_EQ(result->header.metadata.at("_source_path"), "/assets/sounds/boom.wav");
    EXPECT_EQ(result->header.metadata.at("_source_hash"), "abc123");
    EXPECT_EQ(result->header.metadata.at("artist"), "Test Artist");
    EXPECT_EQ(result->data.remaining(), 5u);
}

// ---- GUID round-trip ----

TEST(AssetBinary, GuidPreserved) {
    UUID original = UUID::generate();

    AssetBinaryHeader header;
    header.guid = original;
    header.type = AssetBinaryType::Shader;

    auto bytes = write_asset_binary(header, {});
    auto result = read_asset_binary(bytes.data(), bytes.size());
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->header.guid, original);

    // Also verify the string round-trip
    EXPECT_EQ(result->header.guid.to_string(), original.to_string());
}

// ---- Invalid magic detection ----

TEST(AssetBinary, InvalidMagicReturnsNullopt) {
    std::vector<uint8_t> garbage = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    auto result = read_asset_binary(garbage.data(), garbage.size());
    EXPECT_FALSE(result.has_value());
}

TEST(AssetBinary, EmptyBufferReturnsNullopt) {
    auto result = read_asset_binary(nullptr, 0);
    EXPECT_FALSE(result.has_value());
}

TEST(AssetBinary, TooSmallBufferReturnsNullopt) {
    // Less than 8 bytes (magic size)
    std::vector<uint8_t> small = {0x48, 0x4C, 0x41};
    auto result = read_asset_binary(small.data(), small.size());
    EXPECT_FALSE(result.has_value());
}

// ---- read_asset_header ----

TEST(AssetBinary, ReadHeaderOnly) {
    AssetBinaryHeader header;
    header.guid    = UUID::generate();
    header.type    = AssetBinaryType::Material;
    header.version = 5;
    header.metadata["key"] = "value";

    std::vector<uint8_t> big_payload(10000, 0xAB);
    auto bytes = write_asset_binary(header, big_payload);

    auto hdr = read_asset_header(bytes.data(), bytes.size());
    ASSERT_TRUE(hdr.has_value());
    EXPECT_EQ(hdr->guid, header.guid);
    EXPECT_EQ(hdr->type, AssetBinaryType::Material);
    EXPECT_EQ(hdr->version, 5u);
    EXPECT_EQ(hdr->metadata.at("key"), "value");
}

TEST(AssetBinary, ReadHeaderInvalidMagic) {
    std::vector<uint8_t> garbage = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    auto hdr = read_asset_header(garbage.data(), garbage.size());
    EXPECT_FALSE(hdr.has_value());
}

// ---- Magic constant ----

TEST(AssetBinary, MagicIsCorrect) {
    // ASSET_BINARY_MAGIC should decode to "HLASSET\0" in ASCII as LE uint64_t
    const uint8_t* p = reinterpret_cast<const uint8_t*>(&ASSET_BINARY_MAGIC);
    EXPECT_EQ(p[0], 'H');
    EXPECT_EQ(p[1], 'L');
    EXPECT_EQ(p[2], 'A');
    EXPECT_EQ(p[3], 'S');
    EXPECT_EQ(p[4], 'S');
    EXPECT_EQ(p[5], 'E');
    EXPECT_EQ(p[6], 'T');
    EXPECT_EQ(p[7], '\0');
}

// ---- AssetBinaryType enum ----

TEST(AssetBinary, TypeEnumValues) {
    EXPECT_EQ(static_cast<uint32_t>(AssetBinaryType::Unknown), 0u);
    EXPECT_EQ(static_cast<uint32_t>(AssetBinaryType::Mesh), 1u);
    EXPECT_EQ(static_cast<uint32_t>(AssetBinaryType::Texture), 2u);
    EXPECT_EQ(static_cast<uint32_t>(AssetBinaryType::CubeMap), 3u);
    EXPECT_EQ(static_cast<uint32_t>(AssetBinaryType::Audio), 4u);
    EXPECT_EQ(static_cast<uint32_t>(AssetBinaryType::Shader), 5u);
    EXPECT_EQ(static_cast<uint32_t>(AssetBinaryType::Material), 6u);
    EXPECT_EQ(static_cast<uint32_t>(AssetBinaryType::Scene), 7u);
    EXPECT_EQ(static_cast<uint32_t>(AssetBinaryType::Custom), 1000u);
}

// ---- Large metadata ----

TEST(AssetBinary, ManyMetadataEntries) {
    AssetBinaryHeader header;
    header.guid = UUID::generate();
    header.type = AssetBinaryType::Scene;

    for (int i = 0; i < 100; i++) {
        header.metadata["key_" + std::to_string(i)] = "value_" + std::to_string(i);
    }

    auto bytes = write_asset_binary(header, {0x01});
    auto result = read_asset_binary(bytes.data(), bytes.size());
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->header.metadata.size(), 100u);

    for (int i = 0; i < 100; i++) {
        EXPECT_EQ(result->header.metadata.at("key_" + std::to_string(i)),
                  "value_" + std::to_string(i));
    }
}

} // namespace helios::test
