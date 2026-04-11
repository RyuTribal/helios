#include <gtest/gtest.h>

#include "helios/ecs/asset_handle.h"
#include "helios/assets/asset_server.h"
#include "helios/assets/load_batch.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <unordered_map>

namespace helios::test {

// Simple test asset type
struct TestAsset {
    std::string content;
};

class AssetServerTest : public ::testing::Test {
protected:
    void SetUp() override {
        m_test_dir = std::filesystem::temp_directory_path() / "helios_test_assets";
        std::filesystem::create_directories(m_test_dir);

        {
            std::ofstream f(m_test_dir / "test.txt");
            f << "hello world";
        }
        {
            std::ofstream f(m_test_dir / "test2.txt");
            f << "second file";
        }
    }

    void TearDown() override {
        std::filesystem::remove_all(m_test_dir);
    }

    void register_test_importer(AssetServer& server) {
        server.register_importer<TestAsset>(
            [](const std::filesystem::path& path, AssetServer& /*server*/) -> std::any {
                std::ifstream f(path);
                if (!f.is_open()) {
                    return std::any{};
                }
                std::string content(
                    (std::istreambuf_iterator<char>(f)),
                    std::istreambuf_iterator<char>());
                return std::any(TestAsset{std::move(content)});
            });
    }

    std::filesystem::path m_test_dir;
};

// ----- AssetHandle tests -----

TEST(AssetHandleTest, DefaultIsNull) {
    AssetHandle h;
    EXPECT_FALSE(static_cast<bool>(h));
    EXPECT_EQ(h.index, 0u);
    EXPECT_EQ(h.generation, 0u);
}

TEST(AssetHandleTest, NonZeroIsValid) {
    AssetHandle h{42, 1};
    EXPECT_TRUE(static_cast<bool>(h));
}

TEST(AssetHandleTest, Equality) {
    AssetHandle a{1, 1};
    AssetHandle b{1, 1};
    AssetHandle c{2, 1};
    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
}

TEST(AssetHandleTest, HashWorks) {
    std::unordered_map<AssetHandle, int> map;
    AssetHandle h{99, 1};
    map[h] = 42;
    EXPECT_EQ(map[h], 42);
}

TEST(AssetHandleTest, Ordering) {
    AssetHandle a{1, 1};
    AssetHandle b{2, 1};
    EXPECT_LT(a, b);
    EXPECT_GT(b, a);
}

TEST(AssetHandleTest, PackedRoundTrip) {
    AssetHandle h{42, 7};
    uint64_t packed = h.packed();
    AssetHandle unpacked = AssetHandle::from_packed(packed);
    EXPECT_EQ(h, unpacked);
}

// ----- AssetStatus tests -----

TEST(AssetStatusTest, EnumValues) {
    EXPECT_NE(AssetStatus::Loading, AssetStatus::Loaded);
    EXPECT_NE(AssetStatus::Loading, AssetStatus::Failed);
    EXPECT_NE(AssetStatus::Loaded, AssetStatus::Failed);
}

// ----- Sync loading -----

TEST_F(AssetServerTest, SyncLoadSucceeds) {
    AssetServer server(m_test_dir, 1);
    register_test_importer(server);

    auto handle = server.load_sync<TestAsset>("test.txt");
    ASSERT_TRUE(static_cast<bool>(handle));
    EXPECT_EQ(server.status(handle.untyped()), AssetStatus::Loaded);
    EXPECT_TRUE(server.is_loaded(handle.untyped()));

    const TestAsset* asset = server.get<TestAsset>(handle.untyped());
    ASSERT_NE(asset, nullptr);
    EXPECT_EQ(asset->content, "hello world");
}

TEST_F(AssetServerTest, SyncLoadMissingFileFails) {
    AssetServer server(m_test_dir, 1);
    register_test_importer(server);

    auto handle = server.load_sync<TestAsset>("nonexistent.txt");
    EXPECT_FALSE(static_cast<bool>(handle));
}

TEST_F(AssetServerTest, SyncLoadNoImporterFails) {
    AssetServer server(m_test_dir, 1);
    // No importer registered

    struct UnknownType { int x; };
    auto handle = server.load_sync<UnknownType>("test.txt");
    EXPECT_FALSE(static_cast<bool>(handle));
}

// ----- Async loading -----

TEST_F(AssetServerTest, AsyncLoadCompletesAndResolves) {
    AssetServer server(m_test_dir, 2);
    register_test_importer(server);

    auto handle = server.load<TestAsset>("test.txt");
    ASSERT_TRUE(static_cast<bool>(handle));

    auto start = std::chrono::steady_clock::now();
    while (!server.is_loaded(handle.untyped())) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        auto elapsed = std::chrono::steady_clock::now() - start;
        ASSERT_LT(elapsed, std::chrono::seconds(5))
            << "Async load timed out";
    }

    const TestAsset* asset = server.get<TestAsset>(handle.untyped());
    ASSERT_NE(asset, nullptr);
    EXPECT_EQ(asset->content, "hello world");
}

TEST_F(AssetServerTest, AsyncLoadEmitsCompletedEvent) {
    AssetServer server(m_test_dir, 1);
    register_test_importer(server);

    auto handle = server.load<TestAsset>("test.txt");

    auto start = std::chrono::steady_clock::now();
    while (!server.is_loaded(handle.untyped())) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        auto elapsed = std::chrono::steady_clock::now() - start;
        ASSERT_LT(elapsed, std::chrono::seconds(5));
    }

    auto completed = server.drain_completed();
    ASSERT_FALSE(completed.empty());

    bool found = false;
    for (const auto& event : completed) {
        if (event.handle == handle.untyped()) {
            found = true;
            EXPECT_TRUE(event.success);
        }
    }
    EXPECT_TRUE(found) << "AssetLoaded event for handle not found";
}

TEST_F(AssetServerTest, AsyncLoadFailureStatus) {
    AssetServer server(m_test_dir, 1);
    register_test_importer(server);

    auto handle = server.load<TestAsset>("does_not_exist.txt");

    auto start = std::chrono::steady_clock::now();
    while (server.status(handle.untyped()) == AssetStatus::Loading) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        auto elapsed = std::chrono::steady_clock::now() - start;
        ASSERT_LT(elapsed, std::chrono::seconds(5));
    }

    EXPECT_EQ(server.status(handle.untyped()), AssetStatus::Failed);
    EXPECT_EQ(server.get<TestAsset>(handle.untyped()), nullptr);
}

// ----- Cache deduplication -----

TEST_F(AssetServerTest, DuplicateLoadReturnsSameHandle) {
    AssetServer server(m_test_dir, 1);
    register_test_importer(server);

    auto h1 = server.load<TestAsset>("test.txt");
    auto h2 = server.load<TestAsset>("test.txt");
    EXPECT_EQ(h1, h2);
}

// ----- Batch loading -----

TEST_F(AssetServerTest, BatchLoadProgress) {
    AssetServer server(m_test_dir, 2);
    register_test_importer(server);

    auto batch = server.load_batch()
        .add<TestAsset>("test.txt")
        .add<TestAsset>("test2.txt")
        .submit();

    EXPECT_EQ(batch.total(), 2);

    auto start = std::chrono::steady_clock::now();
    while (!batch.is_complete()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        auto elapsed = std::chrono::steady_clock::now() - start;
        ASSERT_LT(elapsed, std::chrono::seconds(5));
    }

    EXPECT_FLOAT_EQ(batch.progress(), 1.0f);
    EXPECT_EQ(batch.remaining(), 0);
    EXPECT_TRUE(batch.failed().empty());

    for (const auto& h : batch.handles()) {
        EXPECT_TRUE(server.is_loaded(h));
    }
}

TEST_F(AssetServerTest, BatchLoadWithFailure) {
    AssetServer server(m_test_dir, 1);
    register_test_importer(server);

    auto batch = server.load_batch()
        .add<TestAsset>("test.txt")
        .add<TestAsset>("missing.txt")
        .submit();

    EXPECT_EQ(batch.total(), 2);

    auto start = std::chrono::steady_clock::now();
    while (!batch.is_complete()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        auto elapsed = std::chrono::steady_clock::now() - start;
        ASSERT_LT(elapsed, std::chrono::seconds(5));
    }

    EXPECT_TRUE(batch.is_complete());
    auto failures = batch.failed();
    EXPECT_EQ(failures.size(), 1u);
    EXPECT_NE(failures[0].find("missing.txt"), std::string::npos);
}

TEST_F(AssetServerTest, EmptyBatchIsComplete) {
    AssetServer server(m_test_dir, 1);
    register_test_importer(server);

    auto batch = server.load_batch().submit();
    EXPECT_EQ(batch.total(), 0);
    EXPECT_TRUE(batch.is_complete());
    EXPECT_FLOAT_EQ(batch.progress(), 1.0f);
}

// ----- Wrong type get returns nullptr -----

TEST_F(AssetServerTest, GetWithWrongTypeReturnsNull) {
    struct OtherAsset { int x; };

    AssetServer server(m_test_dir, 1);
    register_test_importer(server);

    auto handle = server.load_sync<TestAsset>("test.txt");
    ASSERT_TRUE(static_cast<bool>(handle));

    const OtherAsset* wrong = server.get<OtherAsset>(handle.untyped());
    EXPECT_EQ(wrong, nullptr);
}

// ----- Mutable access -----

TEST_F(AssetServerTest, GetMutAllowsModification) {
    AssetServer server(m_test_dir, 1);
    register_test_importer(server);

    auto handle = server.load_sync<TestAsset>("test.txt");
    ASSERT_TRUE(static_cast<bool>(handle));

    TestAsset* asset = server.get_mut<TestAsset>(handle.untyped());
    ASSERT_NE(asset, nullptr);
    asset->content = "modified";

    const TestAsset* check = server.get<TestAsset>(handle.untyped());
    EXPECT_EQ(check->content, "modified");
}

// ----- Root path -----

TEST_F(AssetServerTest, RootPathMatchesConstruction) {
    AssetServer server(m_test_dir, 1);
    EXPECT_EQ(server.root(), m_test_dir);
}

// ----- Drain returns empty when no completions -----

TEST_F(AssetServerTest, DrainEmptyWhenNothingLoaded) {
    AssetServer server(m_test_dir, 1);
    auto completed = server.drain_completed();
    EXPECT_TRUE(completed.empty());
}

// ----- Refcount and GC -----

TEST_F(AssetServerTest, AcquireAndRelease) {
    AssetServer server(m_test_dir, 1);
    register_test_importer(server);

    auto handle = server.load_sync<TestAsset>("test.txt");
    ASSERT_TRUE(static_cast<bool>(handle));
    auto raw = handle.untyped();

    // Handle constructor already acquired once. Verify.
    EXPECT_EQ(server.refcount(raw), 1u);

    server.acquire(raw);
    EXPECT_EQ(server.refcount(raw), 2u);

    server.release(raw);
    EXPECT_EQ(server.refcount(raw), 1u);

    // The RAII Handle still holds one refcount; release it manually for
    // this test to confirm the count reaches zero.
    server.release(raw);
    EXPECT_EQ(server.refcount(raw), 0u);
}

TEST_F(AssetServerTest, CollectGarbageUnloadsZeroRefcount) {
    AssetServer server(m_test_dir, 1);
    register_test_importer(server);

    auto h1 = server.load_sync<TestAsset>("test.txt");
    auto h2 = server.load_sync<TestAsset>("test2.txt");
    ASSERT_TRUE(static_cast<bool>(h1));
    ASSERT_TRUE(static_cast<bool>(h2));
    auto r1 = h1.untyped();
    auto r2 = h2.untyped();

    // Each Handle already holds refcount=1 from construction.
    // Release h1's refcount so it can be collected.
    server.release(r1);

    auto unloaded = server.collect_garbage();
    EXPECT_EQ(unloaded.size(), 1u);
    EXPECT_EQ(unloaded[0], r1);

    // h1 should be gone, h2 should still be there
    EXPECT_EQ(server.get<TestAsset>(r1), nullptr);
    EXPECT_NE(server.get<TestAsset>(r2), nullptr);
}

TEST_F(AssetServerTest, CollectGarbageNothingToCollect) {
    AssetServer server(m_test_dir, 1);
    register_test_importer(server);

    auto handle = server.load_sync<TestAsset>("test.txt");
    ASSERT_TRUE(static_cast<bool>(handle));

    // Handle already holds refcount=1 from RAII construction
    auto unloaded = server.collect_garbage();
    EXPECT_TRUE(unloaded.empty());
    EXPECT_NE(server.get<TestAsset>(handle.untyped()), nullptr);
}

TEST_F(AssetServerTest, ExplicitUnload) {
    AssetServer server(m_test_dir, 1);
    register_test_importer(server);

    auto handle = server.load_sync<TestAsset>("test.txt");
    ASSERT_TRUE(static_cast<bool>(handle));

    server.unload(handle.untyped());
    EXPECT_EQ(server.get<TestAsset>(handle.untyped()), nullptr);
    EXPECT_EQ(server.status(handle.untyped()), AssetStatus::Failed);
}

TEST_F(AssetServerTest, SharedAssetNotCollected) {
    AssetServer server(m_test_dir, 1);
    register_test_importer(server);

    auto handle = server.load_sync<TestAsset>("test.txt");
    ASSERT_TRUE(static_cast<bool>(handle));
    auto raw = handle.untyped();

    // Handle already holds refcount=1. Acquire again for a second "owner".
    server.acquire(raw);
    EXPECT_EQ(server.refcount(raw), 2u);

    // One releases
    server.release(raw);
    EXPECT_EQ(server.refcount(raw), 1u);

    // GC should not collect (refcount = 1)
    auto unloaded = server.collect_garbage();
    EXPECT_TRUE(unloaded.empty());
    EXPECT_NE(server.get<TestAsset>(raw), nullptr);
}

} // namespace helios::test
