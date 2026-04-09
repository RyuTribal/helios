#include <gtest/gtest.h>

#include "helios/ecs/asset_handle.h"
#include "helios/assets/asset_server.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <typeindex>

namespace helios::test {

// Test asset types for extension mapping
struct ExtTestMesh { std::string data; };
struct ExtTestTexture { std::string data; };
struct ExtTestAudio { std::string data; };

class AssetExtensionTest : public ::testing::Test {
protected:
    void SetUp() override {
        m_test_dir = std::filesystem::temp_directory_path() / "helios_ext_test";
        std::filesystem::create_directories(m_test_dir);

        // Create test files with various extensions
        for (const auto& name : {
            "model.gltf", "model.glb", "model.GLTF",
            "image.png", "image.jpg", "image.TGA",
            "sound.wav", "sound.ogg",
            "noext", "dots.only."
        }) {
            std::ofstream f(m_test_dir / name);
            f << "test content for " << name;
        }
    }

    void TearDown() override {
        std::filesystem::remove_all(m_test_dir);
    }

    void register_importers(AssetServer& server) {
        server.register_importer<ExtTestMesh>(
            [](const std::filesystem::path& path, AssetServer&) -> std::any {
                std::ifstream f(path);
                if (!f.is_open()) throw std::runtime_error("Cannot open: " + path.string());
                std::string content((std::istreambuf_iterator<char>(f)),
                                    std::istreambuf_iterator<char>());
                return std::any(ExtTestMesh{std::move(content)});
            });
        server.register_importer<ExtTestTexture>(
            [](const std::filesystem::path& path, AssetServer&) -> std::any {
                std::ifstream f(path);
                if (!f.is_open()) throw std::runtime_error("Cannot open: " + path.string());
                std::string content((std::istreambuf_iterator<char>(f)),
                                    std::istreambuf_iterator<char>());
                return std::any(ExtTestTexture{std::move(content)});
            });
        server.register_importer<ExtTestAudio>(
            [](const std::filesystem::path& path, AssetServer&) -> std::any {
                std::ifstream f(path);
                if (!f.is_open()) throw std::runtime_error("Cannot open: " + path.string());
                std::string content((std::istreambuf_iterator<char>(f)),
                                    std::istreambuf_iterator<char>());
                return std::any(ExtTestAudio{std::move(content)});
            });
    }

    void register_extensions(AssetServer& server) {
        server.register_extensions<ExtTestMesh>({"gltf", "glb"});
        server.register_extensions<ExtTestTexture>({"png", "jpg", "jpeg", "tga"});
        server.register_extensions<ExtTestAudio>({"wav", "ogg"});
    }

    std::filesystem::path m_test_dir;
};

// ----- register_extensions / type_for_extension -----

TEST_F(AssetExtensionTest, RegisterAndQueryExtension) {
    AssetServer server(m_test_dir, 0);
    register_importers(server);
    register_extensions(server);

    auto type = server.type_for_extension("gltf");
    ASSERT_TRUE(type.has_value());
    EXPECT_EQ(*type, std::type_index(typeid(ExtTestMesh)));
}

TEST_F(AssetExtensionTest, ExtensionNormalization) {
    AssetServer server(m_test_dir, 0);
    register_importers(server);
    register_extensions(server);

    // Uppercase should match
    auto type1 = server.type_for_extension("GLTF");
    ASSERT_TRUE(type1.has_value());
    EXPECT_EQ(*type1, std::type_index(typeid(ExtTestMesh)));

    // Leading dot should be stripped
    auto type2 = server.type_for_extension(".png");
    ASSERT_TRUE(type2.has_value());
    EXPECT_EQ(*type2, std::type_index(typeid(ExtTestTexture)));

    // Mixed case with dot
    auto type3 = server.type_for_extension(".WAV");
    ASSERT_TRUE(type3.has_value());
    EXPECT_EQ(*type3, std::type_index(typeid(ExtTestAudio)));
}

TEST_F(AssetExtensionTest, UnknownExtensionReturnsNullopt) {
    AssetServer server(m_test_dir, 0);
    register_importers(server);
    register_extensions(server);

    EXPECT_FALSE(server.type_for_extension("xyz").has_value());
    EXPECT_FALSE(server.type_for_extension("").has_value());
}

// ----- load_sync_by_extension -----

TEST_F(AssetExtensionTest, LoadSyncByExtensionMesh) {
    AssetServer server(m_test_dir, 0);
    register_importers(server);
    register_extensions(server);

    auto handle = server.load_sync_by_extension("model.gltf");
    ASSERT_TRUE(static_cast<bool>(handle));
    EXPECT_EQ(server.status(handle), AssetStatus::Loaded);

    const auto* asset = server.get<ExtTestMesh>(handle);
    ASSERT_NE(asset, nullptr);
    EXPECT_FALSE(asset->data.empty());
}

TEST_F(AssetExtensionTest, LoadSyncByExtensionTexture) {
    AssetServer server(m_test_dir, 0);
    register_importers(server);
    register_extensions(server);

    auto handle = server.load_sync_by_extension("image.png");
    ASSERT_TRUE(static_cast<bool>(handle));

    const auto* asset = server.get<ExtTestTexture>(handle);
    ASSERT_NE(asset, nullptr);
    EXPECT_FALSE(asset->data.empty());
}

TEST_F(AssetExtensionTest, LoadSyncByExtensionUnknownReturnsNull) {
    AssetServer server(m_test_dir, 0);
    register_importers(server);
    register_extensions(server);

    auto handle = server.load_sync_by_extension("file.xyz");
    EXPECT_FALSE(static_cast<bool>(handle));
}

TEST_F(AssetExtensionTest, LoadSyncByExtensionNoExtReturnsNull) {
    AssetServer server(m_test_dir, 0);
    register_importers(server);
    register_extensions(server);

    auto handle = server.load_sync_by_extension("noext");
    EXPECT_FALSE(static_cast<bool>(handle));
}

// ----- load_by_extension (async) -----

TEST_F(AssetExtensionTest, LoadByExtensionAsync) {
    AssetServer server(m_test_dir, 1);
    register_importers(server);
    register_extensions(server);

    auto handle = server.load_by_extension("sound.wav");
    ASSERT_TRUE(static_cast<bool>(handle));

    // Wait for load to complete
    auto start = std::chrono::steady_clock::now();
    while (server.status(handle) == AssetStatus::Loading) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        auto elapsed = std::chrono::steady_clock::now() - start;
        ASSERT_LT(elapsed, std::chrono::seconds(5)) << "Async load timed out";
    }

    EXPECT_EQ(server.status(handle), AssetStatus::Loaded);
    const auto* asset = server.get<ExtTestAudio>(handle);
    ASSERT_NE(asset, nullptr);
    EXPECT_FALSE(asset->data.empty());
}

// ----- Refcount semantics -----

TEST_F(AssetExtensionTest, LoadByExtensionAcquiresRefcount) {
    AssetServer server(m_test_dir, 0);
    register_importers(server);
    register_extensions(server);

    auto handle = server.load_sync_by_extension("model.gltf");
    ASSERT_TRUE(static_cast<bool>(handle));

    // load_internal acquires once
    EXPECT_EQ(server.refcount(handle), 1u);

    // Release and verify
    server.release(handle);
    EXPECT_EQ(server.refcount(handle), 0u);
}

// ----- Multiple extensions for same type -----

TEST_F(AssetExtensionTest, MultipleExtensionsSameType) {
    AssetServer server(m_test_dir, 0);
    register_importers(server);
    register_extensions(server);

    auto type_gltf = server.type_for_extension("gltf");
    auto type_glb  = server.type_for_extension("glb");
    ASSERT_TRUE(type_gltf.has_value());
    ASSERT_TRUE(type_glb.has_value());
    EXPECT_EQ(*type_gltf, *type_glb);
}

} // namespace helios::test
