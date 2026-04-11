#include <gtest/gtest.h>

#include "helios/assets/importers/texture_importer.h"
#include "helios/assets/importers/mesh_importer.h"
#include "helios/assets/importers/audio_importer.h"
#include "helios/assets/asset_server.h"

#include <any>
#include <filesystem>
#include <fstream>

namespace helios::test {

// Helper: create a temporary AssetServer for importer tests
static AssetServer make_test_server() {
    auto tmp = std::filesystem::temp_directory_path() / "helios_import_test";
    std::filesystem::create_directories(tmp);
    return AssetServer(tmp, 0);
}

// ============================================================================
// TextureImporter tests
// ============================================================================

TEST(TextureImporterTest, MissingFileReturnsEmpty) {
    auto server = make_test_server();
    auto result = TextureImporter::import_ldr("/nonexistent/texture.png", server);
    EXPECT_FALSE(result.has_value());
}

TEST(TextureImporterTest, MissingHdrFileReturnsEmpty) {
    auto server = make_test_server();
    auto result = TextureImporter::import_hdr("/nonexistent/env.hdr", server);
    EXPECT_FALSE(result.has_value());
}

TEST(TextureImporterTest, InvalidDataReturnsEmpty) {
    auto tmp = std::filesystem::temp_directory_path() / "helios_bad_tex.png";
    {
        std::ofstream f(tmp, std::ios::binary);
        f << "not a real image file";
    }

    auto server = make_test_server();
    auto result = TextureImporter::import_ldr(tmp, server);
    EXPECT_FALSE(result.has_value());

    std::filesystem::remove(tmp);
}

TEST(TextureImporterTest, InvalidHdrDataReturnsEmpty) {
    auto tmp = std::filesystem::temp_directory_path() / "helios_bad_tex.hdr";
    {
        std::ofstream f(tmp, std::ios::binary);
        f << "not a real HDR file";
    }

    auto server = make_test_server();
    auto result = TextureImporter::import_hdr(tmp, server);
    EXPECT_FALSE(result.has_value());

    std::filesystem::remove(tmp);
}

// ============================================================================
// MeshImporter tests
// ============================================================================

TEST(MeshImporterTest, MissingFileReturnsEmpty) {
    auto server = make_test_server();
    auto result = MeshImporter::import("/nonexistent/model.gltf", server);
    EXPECT_FALSE(result.has_value());
}

TEST(MeshImporterTest, InvalidDataReturnsEmpty) {
    auto tmp = std::filesystem::temp_directory_path() / "helios_bad_mesh.gltf";
    {
        std::ofstream f(tmp);
        f << "{ this is not valid gltf }";
    }

    auto server = make_test_server();
    auto result = MeshImporter::import(tmp, server);
    EXPECT_FALSE(result.has_value());

    std::filesystem::remove(tmp);
}

// ============================================================================
// AudioImporter tests
// ============================================================================

TEST(AudioImporterTest, MissingFileReturnsEmpty) {
    auto server = make_test_server();
    auto result = AudioImporter::import("/nonexistent/audio.wav", server);
    EXPECT_FALSE(result.has_value());
}

TEST(AudioImporterTest, EmptyFileReturnsEmpty) {
    auto tmp = std::filesystem::temp_directory_path() / "helios_empty_audio.wav";
    {
        std::ofstream f(tmp, std::ios::binary);
        // write nothing
    }

    auto server = make_test_server();
    auto result = AudioImporter::import(tmp, server);
    EXPECT_FALSE(result.has_value());

    std::filesystem::remove(tmp);
}

TEST(AudioImporterTest, ValidFileReturnsAudioData) {
    auto tmp = std::filesystem::temp_directory_path() / "helios_test_audio.wav";
    {
        std::ofstream f(tmp, std::ios::binary);
        // Write some dummy bytes -- AudioImporter just reads raw bytes.
        const char data[] = "RIFF....WAVEfmt dummy data for test";
        f.write(data, sizeof(data) - 1);
    }

    auto server = make_test_server();
    auto result = AudioImporter::import(tmp, server);
    ASSERT_TRUE(result.has_value());

    auto& audio = std::any_cast<AudioData&>(result);
    EXPECT_TRUE(audio.is_valid());
    EXPECT_EQ(audio.format, AudioFormat::WAV);
    EXPECT_FALSE(audio.source_path.empty());

    std::filesystem::remove(tmp);
}

TEST(AudioImporterTest, FormatDetection) {
    EXPECT_EQ(AudioImporter::format_from_extension(".wav"),  AudioFormat::WAV);
    EXPECT_EQ(AudioImporter::format_from_extension(".WAV"),  AudioFormat::WAV);
    EXPECT_EQ(AudioImporter::format_from_extension(".ogg"),  AudioFormat::OGG);
    EXPECT_EQ(AudioImporter::format_from_extension(".mp3"),  AudioFormat::MP3);
    EXPECT_EQ(AudioImporter::format_from_extension(".flac"), AudioFormat::FLAC);
    EXPECT_EQ(AudioImporter::format_from_extension(".xyz"),  AudioFormat::Unknown);
}

} // namespace helios::test
