#include <gtest/gtest.h>

#include "helios/assets/asset_server.h"
#include "helios/assets/asset_binary.h"
#include "helios/core/uuid.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace helios::test {

class AssetImportExportTest : public ::testing::Test {
protected:
    void SetUp() override {
        m_test_dir = std::filesystem::temp_directory_path() / "helios_import_export_test";
        std::filesystem::create_directories(m_test_dir);

        // Write a fake source file
        {
            std::ofstream f(m_test_dir / "model.glb", std::ios::binary);
            f << "FAKE_GLB_DATA_12345";
        }
        {
            std::ofstream f(m_test_dir / "texture.png", std::ios::binary);
            f << "FAKE_PNG_DATA";
        }
    }

    void TearDown() override {
        std::filesystem::remove_all(m_test_dir);
    }

    /// A trivial importer that just passes source bytes through as asset data.
    static std::vector<uint8_t> passthrough_importer(
        const std::vector<uint8_t>& source_bytes,
        const AssetMetadata& /*metadata*/,
        AssetServer& /*server*/) {
        return source_bytes;
    }

    /// A trivial exporter that just passes asset data through.
    static std::vector<uint8_t> passthrough_exporter(
        const std::vector<uint8_t>& asset_data,
        const AssetMetadata& /*metadata*/) {
        return asset_data;
    }

    /// An importer that prepends a marker to the data.
    static std::vector<uint8_t> marker_importer(
        const std::vector<uint8_t>& source_bytes,
        const AssetMetadata& /*metadata*/,
        AssetServer& /*server*/) {
        std::vector<uint8_t> result;
        std::string marker = "IMPORTED:";
        result.insert(result.end(), marker.begin(), marker.end());
        result.insert(result.end(), source_bytes.begin(), source_bytes.end());
        return result;
    }

    std::filesystem::path m_test_dir;
};

// ---- register_asset_importer / get_import_settings / detect_import_type ----

TEST_F(AssetImportExportTest, RegisterImporterAndQuerySettings) {
    AssetServer server(m_test_dir, 0);

    AssetImportSettings settings;
    settings.default_metadata["quality"] = "high";
    settings.source_extensions = {"glb", "gltf", "fbx"};
    settings.export_formats = {"glb", "gltf"};

    server.register_asset_importer(
        AssetBinaryType::Mesh, passthrough_importer, settings);

    // Check settings retrieval
    auto* retrieved = server.get_import_settings(AssetBinaryType::Mesh);
    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->type, AssetBinaryType::Mesh);
    EXPECT_EQ(retrieved->source_extensions.size(), 3u);
    EXPECT_EQ(retrieved->export_formats.size(), 2u);
    EXPECT_EQ(retrieved->default_metadata.at("quality"), "high");
}

TEST_F(AssetImportExportTest, GetImportSettingsForUnregisteredTypeReturnsNull) {
    AssetServer server(m_test_dir, 0);

    auto* settings = server.get_import_settings(AssetBinaryType::Audio);
    EXPECT_EQ(settings, nullptr);
}

TEST_F(AssetImportExportTest, DetectImportType) {
    AssetServer server(m_test_dir, 0);

    AssetImportSettings settings;
    settings.source_extensions = {"glb", "gltf"};
    server.register_asset_importer(
        AssetBinaryType::Mesh, passthrough_importer, settings);

    AssetImportSettings tex_settings;
    tex_settings.source_extensions = {"png", "jpg", "tga"};
    server.register_asset_importer(
        AssetBinaryType::Texture, passthrough_importer, tex_settings);

    auto mesh_type = server.detect_import_type("glb");
    ASSERT_TRUE(mesh_type.has_value());
    EXPECT_EQ(*mesh_type, AssetBinaryType::Mesh);

    auto gltf_type = server.detect_import_type("gltf");
    ASSERT_TRUE(gltf_type.has_value());
    EXPECT_EQ(*gltf_type, AssetBinaryType::Mesh);

    auto png_type = server.detect_import_type("png");
    ASSERT_TRUE(png_type.has_value());
    EXPECT_EQ(*png_type, AssetBinaryType::Texture);

    // Unknown extension
    auto unknown = server.detect_import_type("xyz");
    EXPECT_FALSE(unknown.has_value());
}

TEST_F(AssetImportExportTest, DetectImportTypeNormalizesExtension) {
    AssetServer server(m_test_dir, 0);

    AssetImportSettings settings;
    settings.source_extensions = {"glb"};
    server.register_asset_importer(
        AssetBinaryType::Mesh, passthrough_importer, settings);

    // Leading dot and uppercase should be normalized
    auto type1 = server.detect_import_type(".GLB");
    ASSERT_TRUE(type1.has_value());
    EXPECT_EQ(*type1, AssetBinaryType::Mesh);

    auto type2 = server.detect_import_type("Glb");
    ASSERT_TRUE(type2.has_value());
    EXPECT_EQ(*type2, AssetBinaryType::Mesh);
}

// ---- import_asset ----

TEST_F(AssetImportExportTest, ImportAssetCreatesFile) {
    AssetServer server(m_test_dir, 0);

    AssetImportSettings settings;
    settings.source_extensions = {"glb"};
    server.register_asset_importer(
        AssetBinaryType::Mesh, passthrough_importer, settings);

    auto dest = server.import_asset("model.glb", AssetBinaryType::Mesh,
                                     "assets/model.hlasset");

    EXPECT_EQ(dest, "assets/model.hlasset");

    // Verify file was created
    auto full_path = m_test_dir / "assets" / "model.hlasset";
    EXPECT_TRUE(std::filesystem::exists(full_path));

    // Read it back and verify the header
    std::ifstream f(full_path, std::ios::binary | std::ios::ate);
    auto size = f.tellg();
    f.seekg(0, std::ios::beg);
    std::vector<uint8_t> bytes(static_cast<size_t>(size));
    f.read(reinterpret_cast<char*>(bytes.data()), size);

    auto result = read_asset_binary(bytes.data(), bytes.size());
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->header.type, AssetBinaryType::Mesh);
    EXPECT_TRUE(static_cast<bool>(result->header.guid)); // non-nil
    EXPECT_EQ(result->header.metadata.at("_source_path"), "model.glb");
    EXPECT_FALSE(result->header.metadata.at("_source_hash").empty());
    EXPECT_FALSE(result->header.metadata.at("_import_time").empty());
}

TEST_F(AssetImportExportTest, ImportAssetDataPassedThrough) {
    AssetServer server(m_test_dir, 0);

    AssetImportSettings settings;
    settings.source_extensions = {"glb"};
    server.register_asset_importer(
        AssetBinaryType::Mesh, passthrough_importer, settings);

    server.import_asset("model.glb", AssetBinaryType::Mesh,
                        "assets/model.hlasset");

    // Read it back
    auto full_path = m_test_dir / "assets" / "model.hlasset";
    std::ifstream f(full_path, std::ios::binary | std::ios::ate);
    auto size = f.tellg();
    f.seekg(0, std::ios::beg);
    std::vector<uint8_t> bytes(static_cast<size_t>(size));
    f.read(reinterpret_cast<char*>(bytes.data()), size);

    auto result = read_asset_binary(bytes.data(), bytes.size());
    ASSERT_TRUE(result.has_value());

    // The payload should be the original file contents
    std::string expected = "FAKE_GLB_DATA_12345";
    EXPECT_EQ(result->data.remaining(), expected.size());

    std::vector<uint8_t> payload(result->data.remaining());
    result->data.read_bytes(payload.data(), payload.size());
    std::string actual(payload.begin(), payload.end());
    EXPECT_EQ(actual, expected);
}

TEST_F(AssetImportExportTest, ImportAssetMergesExtraMetadata) {
    AssetServer server(m_test_dir, 0);

    AssetImportSettings settings;
    settings.source_extensions = {"glb"};
    settings.default_metadata["quality"] = "medium";
    settings.default_metadata["format"] = "default";
    server.register_asset_importer(
        AssetBinaryType::Mesh, passthrough_importer, settings);

    AssetMetadata extra;
    extra["quality"] = "ultra"; // override default
    extra["artist"] = "test_user";

    server.import_asset("model.glb", AssetBinaryType::Mesh,
                        "assets/model.hlasset", extra);

    auto full_path = m_test_dir / "assets" / "model.hlasset";
    std::ifstream f(full_path, std::ios::binary | std::ios::ate);
    auto size = f.tellg();
    f.seekg(0, std::ios::beg);
    std::vector<uint8_t> bytes(static_cast<size_t>(size));
    f.read(reinterpret_cast<char*>(bytes.data()), size);

    auto result = read_asset_binary(bytes.data(), bytes.size());
    ASSERT_TRUE(result.has_value());

    // Extra metadata should override defaults
    EXPECT_EQ(result->header.metadata.at("quality"), "ultra");
    // Default metadata should still be present
    EXPECT_EQ(result->header.metadata.at("format"), "default");
    // Extra metadata should be present
    EXPECT_EQ(result->header.metadata.at("artist"), "test_user");
}

TEST_F(AssetImportExportTest, ImportAssetAbsoluteSourcePath) {
    AssetServer server(m_test_dir, 0);

    AssetImportSettings settings;
    settings.source_extensions = {"glb"};
    server.register_asset_importer(
        AssetBinaryType::Mesh, passthrough_importer, settings);

    // Use absolute path for source
    auto abs_source = m_test_dir / "model.glb";
    server.import_asset(abs_source, AssetBinaryType::Mesh,
                        "assets/model.hlasset");

    auto full_path = m_test_dir / "assets" / "model.hlasset";
    EXPECT_TRUE(std::filesystem::exists(full_path));
}

TEST_F(AssetImportExportTest, ImportAssetNoImporterReturnsEmpty) {
    AssetServer server(m_test_dir, 0);

    auto result = server.import_asset("model.glb", AssetBinaryType::Mesh,
                            "assets/model.hlasset");
    EXPECT_TRUE(result.empty());
}

TEST_F(AssetImportExportTest, ImportAssetMissingSourceReturnsEmpty) {
    AssetServer server(m_test_dir, 0);

    AssetImportSettings settings;
    settings.source_extensions = {"glb"};
    server.register_asset_importer(
        AssetBinaryType::Mesh, passthrough_importer, settings);

    auto result = server.import_asset("nonexistent.glb", AssetBinaryType::Mesh,
                            "assets/out.hlasset");
    EXPECT_TRUE(result.empty());
}

// ---- reimport_asset ----

TEST_F(AssetImportExportTest, ReimportPreservesGuid) {
    AssetServer server(m_test_dir, 0);

    AssetImportSettings settings;
    settings.source_extensions = {"glb"};
    server.register_asset_importer(
        AssetBinaryType::Mesh, passthrough_importer, settings);

    // Initial import
    server.import_asset("model.glb", AssetBinaryType::Mesh,
                        "assets/model.hlasset");

    // Read original GUID
    UUID original_guid;
    auto full_path = m_test_dir / "assets" / "model.hlasset";
    {
        std::ifstream f(full_path, std::ios::binary | std::ios::ate);
        auto size = f.tellg();
        f.seekg(0, std::ios::beg);
        std::vector<uint8_t> bytes(static_cast<size_t>(size));
        f.read(reinterpret_cast<char*>(bytes.data()), size);

        auto result = read_asset_binary(bytes.data(), bytes.size());
        ASSERT_TRUE(result.has_value());
        original_guid = result->header.guid;
    }

    // Write a new source file
    {
        std::ofstream f(m_test_dir / "model_v2.glb", std::ios::binary);
        f << "UPDATED_GLB_DATA";
    }

    // Re-import
    server.reimport_asset("assets/model.hlasset", "model_v2.glb");

    // Read the re-imported asset
    {
        std::ifstream f(full_path, std::ios::binary | std::ios::ate);
        auto size = f.tellg();
        f.seekg(0, std::ios::beg);
        std::vector<uint8_t> bytes(static_cast<size_t>(size));
        f.read(reinterpret_cast<char*>(bytes.data()), size);

        auto result = read_asset_binary(bytes.data(), bytes.size());
        ASSERT_TRUE(result.has_value());

        // GUID must be preserved
        EXPECT_EQ(result->header.guid, original_guid);
        EXPECT_EQ(result->header.type, AssetBinaryType::Mesh);

        // Source path should be updated
        EXPECT_EQ(result->header.metadata.at("_source_path"), "model_v2.glb");

        // Payload should be new data
        std::string expected = "UPDATED_GLB_DATA";
        EXPECT_EQ(result->data.remaining(), expected.size());

        std::vector<uint8_t> payload(result->data.remaining());
        result->data.read_bytes(payload.data(), payload.size());
        std::string actual(payload.begin(), payload.end());
        EXPECT_EQ(actual, expected);
    }
}

TEST_F(AssetImportExportTest, ReimportMergesExtraMetadata) {
    AssetServer server(m_test_dir, 0);

    AssetImportSettings settings;
    settings.source_extensions = {"glb"};
    server.register_asset_importer(
        AssetBinaryType::Mesh, passthrough_importer, settings);

    // Import with initial metadata
    AssetMetadata initial;
    initial["artist"] = "alice";
    server.import_asset("model.glb", AssetBinaryType::Mesh,
                        "assets/model.hlasset", initial);

    // Re-import with extra metadata
    AssetMetadata extra;
    extra["artist"] = "bob"; // override
    extra["notes"] = "updated";
    server.reimport_asset("assets/model.hlasset", "model.glb", extra);

    auto full_path = m_test_dir / "assets" / "model.hlasset";
    std::ifstream f(full_path, std::ios::binary | std::ios::ate);
    auto size = f.tellg();
    f.seekg(0, std::ios::beg);
    std::vector<uint8_t> bytes(static_cast<size_t>(size));
    f.read(reinterpret_cast<char*>(bytes.data()), size);

    auto result = read_asset_binary(bytes.data(), bytes.size());
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->header.metadata.at("artist"), "bob");
    EXPECT_EQ(result->header.metadata.at("notes"), "updated");
}

TEST_F(AssetImportExportTest, ReimportInvalidAssetThrows) {
    AssetServer server(m_test_dir, 0);

    AssetImportSettings settings;
    settings.source_extensions = {"glb"};
    server.register_asset_importer(
        AssetBinaryType::Mesh, passthrough_importer, settings);

    // Write garbage where asset would be
    {
        std::filesystem::create_directories(m_test_dir / "assets");
        std::ofstream f(m_test_dir / "assets" / "bad.hlasset", std::ios::binary);
        f << "NOT_A_VALID_ASSET";
    }

    // reimport should handle gracefully (log error, return)
    server.reimport_asset("assets/bad.hlasset", "model.glb");
    // No crash = success; the function logs an error and returns.
}

// ---- export_asset ----

TEST_F(AssetImportExportTest, ExportAssetRoundTrip) {
    AssetServer server(m_test_dir, 0);

    AssetImportSettings settings;
    settings.source_extensions = {"glb"};
    server.register_asset_importer(
        AssetBinaryType::Mesh, passthrough_importer, settings);
    server.register_asset_exporter(
        AssetBinaryType::Mesh, "glb", passthrough_exporter);

    // Import
    server.import_asset("model.glb", AssetBinaryType::Mesh,
                        "assets/model.hlasset");

    // Export
    auto exported = server.export_asset("assets/model.hlasset", "glb");

    // The exported bytes should be the original source bytes (since both
    // importer and exporter are passthrough)
    std::string expected = "FAKE_GLB_DATA_12345";
    std::string actual(exported.begin(), exported.end());
    EXPECT_EQ(actual, expected);
}

TEST_F(AssetImportExportTest, ExportAssetNoExporterReturnsEmpty) {
    AssetServer server(m_test_dir, 0);

    AssetImportSettings settings;
    settings.source_extensions = {"glb"};
    server.register_asset_importer(
        AssetBinaryType::Mesh, passthrough_importer, settings);

    server.import_asset("model.glb", AssetBinaryType::Mesh,
                        "assets/model.hlasset");

    // No exporter registered for "obj" format
    auto result = server.export_asset("assets/model.hlasset", "obj");
    EXPECT_TRUE(result.empty());
}

TEST_F(AssetImportExportTest, ExportAssetInvalidFileReturnsEmpty) {
    AssetServer server(m_test_dir, 0);

    server.register_asset_exporter(
        AssetBinaryType::Mesh, "glb", passthrough_exporter);

    auto result = server.export_asset("nonexistent.hlasset", "glb");
    EXPECT_TRUE(result.empty());
}

// ---- register_asset_exporter ----

TEST_F(AssetImportExportTest, MultipleExportersPerType) {
    AssetServer server(m_test_dir, 0);

    AssetImportSettings settings;
    settings.source_extensions = {"glb"};
    server.register_asset_importer(
        AssetBinaryType::Mesh, passthrough_importer, settings);

    // Register two different export formats
    server.register_asset_exporter(
        AssetBinaryType::Mesh, "glb", passthrough_exporter);
    server.register_asset_exporter(
        AssetBinaryType::Mesh, "obj",
        [](const std::vector<uint8_t>& data, const AssetMetadata&) {
            // Simulate conversion by prefixing "OBJ:"
            std::vector<uint8_t> result;
            std::string prefix = "OBJ:";
            result.insert(result.end(), prefix.begin(), prefix.end());
            result.insert(result.end(), data.begin(), data.end());
            return result;
        });

    server.import_asset("model.glb", AssetBinaryType::Mesh,
                        "assets/model.hlasset");

    // Export as glb
    auto glb_export = server.export_asset("assets/model.hlasset", "glb");
    std::string glb_str(glb_export.begin(), glb_export.end());
    EXPECT_EQ(glb_str, "FAKE_GLB_DATA_12345");

    // Export as obj
    auto obj_export = server.export_asset("assets/model.hlasset", "obj");
    std::string obj_str(obj_export.begin(), obj_export.end());
    EXPECT_EQ(obj_str, "OBJ:FAKE_GLB_DATA_12345");
}

// ---- import with custom importer ----

TEST_F(AssetImportExportTest, ImportWithTransformingImporter) {
    AssetServer server(m_test_dir, 0);

    AssetImportSettings settings;
    settings.source_extensions = {"glb"};
    server.register_asset_importer(
        AssetBinaryType::Mesh, marker_importer, settings);

    server.import_asset("model.glb", AssetBinaryType::Mesh,
                        "assets/model.hlasset");

    auto full_path = m_test_dir / "assets" / "model.hlasset";
    std::ifstream f(full_path, std::ios::binary | std::ios::ate);
    auto size = f.tellg();
    f.seekg(0, std::ios::beg);
    std::vector<uint8_t> bytes(static_cast<size_t>(size));
    f.read(reinterpret_cast<char*>(bytes.data()), size);

    auto result = read_asset_binary(bytes.data(), bytes.size());
    ASSERT_TRUE(result.has_value());

    std::string expected = "IMPORTED:FAKE_GLB_DATA_12345";
    EXPECT_EQ(result->data.remaining(), expected.size());

    std::vector<uint8_t> payload(result->data.remaining());
    result->data.read_bytes(payload.data(), payload.size());
    std::string actual(payload.begin(), payload.end());
    EXPECT_EQ(actual, expected);
}

// ---- source hash ----

TEST_F(AssetImportExportTest, SourceHashChangesWithContent) {
    AssetServer server(m_test_dir, 0);

    AssetImportSettings settings;
    settings.source_extensions = {"glb"};
    server.register_asset_importer(
        AssetBinaryType::Mesh, passthrough_importer, settings);

    // Import first file
    server.import_asset("model.glb", AssetBinaryType::Mesh,
                        "assets/model1.hlasset");

    // Import second (different content) file
    server.import_asset("texture.png", AssetBinaryType::Mesh,
                        "assets/model2.hlasset");

    // Read both hashes
    auto read_hash = [&](const std::string& rel_path) -> std::string {
        auto full = m_test_dir / rel_path;
        std::ifstream f(full, std::ios::binary | std::ios::ate);
        auto size = f.tellg();
        f.seekg(0, std::ios::beg);
        std::vector<uint8_t> bytes(static_cast<size_t>(size));
        f.read(reinterpret_cast<char*>(bytes.data()), size);
        auto result = read_asset_binary(bytes.data(), bytes.size());
        return result->header.metadata.at("_source_hash");
    };

    std::string hash1 = read_hash("assets/model1.hlasset");
    std::string hash2 = read_hash("assets/model2.hlasset");
    EXPECT_NE(hash1, hash2);
}

} // namespace helios::test
