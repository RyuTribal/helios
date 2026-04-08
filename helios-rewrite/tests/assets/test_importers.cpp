#include <gtest/gtest.h>

#include "helios/assets/importers/texture_importer.h"
#include "helios/assets/importers/mesh_importer.h"
#include "helios/assets/importers/audio_importer.h"

#include <any>
#include <filesystem>
#include <fstream>

namespace helios::test {

// ============================================================================
// TextureImporter tests
// ============================================================================

TEST(TextureImporterTest, MissingFileThrows) {
    EXPECT_THROW(TextureImporter::import_ldr("/nonexistent/texture.png"),
                 std::runtime_error);
}

TEST(TextureImporterTest, MissingHdrFileThrows) {
    EXPECT_THROW(TextureImporter::import_hdr("/nonexistent/env.hdr"),
                 std::runtime_error);
}

TEST(TextureImporterTest, InvalidDataThrows) {
    auto tmp = std::filesystem::temp_directory_path() / "helios_bad_tex.png";
    {
        std::ofstream f(tmp, std::ios::binary);
        f << "not a real image file";
    }

    EXPECT_THROW(TextureImporter::import_ldr(tmp), std::runtime_error);

    std::filesystem::remove(tmp);
}

TEST(TextureImporterTest, InvalidHdrDataThrows) {
    auto tmp = std::filesystem::temp_directory_path() / "helios_bad_tex.hdr";
    {
        std::ofstream f(tmp, std::ios::binary);
        f << "not a real HDR file";
    }

    EXPECT_THROW(TextureImporter::import_hdr(tmp), std::runtime_error);

    std::filesystem::remove(tmp);
}

// ============================================================================
// MeshImporter tests
// ============================================================================

TEST(MeshImporterTest, MissingFileThrows) {
    EXPECT_THROW(MeshImporter::import("/nonexistent/model.gltf"),
                 std::runtime_error);
}

TEST(MeshImporterTest, InvalidDataThrows) {
    auto tmp = std::filesystem::temp_directory_path() / "helios_bad_mesh.gltf";
    {
        std::ofstream f(tmp);
        f << "{ this is not valid gltf }";
    }

    EXPECT_THROW(MeshImporter::import(tmp), std::runtime_error);

    std::filesystem::remove(tmp);
}

// ============================================================================
// AudioImporter tests
// ============================================================================

TEST(AudioImporterTest, MissingFileThrows) {
    EXPECT_THROW(AudioImporter::import("/nonexistent/audio.wav"),
                 std::runtime_error);
}

TEST(AudioImporterTest, EmptyFileThrows) {
    auto tmp = std::filesystem::temp_directory_path() / "helios_empty_audio.wav";
    {
        std::ofstream f(tmp, std::ios::binary);
        // write nothing
    }

    EXPECT_THROW(AudioImporter::import(tmp), std::runtime_error);

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

    auto result = AudioImporter::import(tmp);
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
