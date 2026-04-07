// helios-rewrite/tests/core/test_logging.cpp
//
// Comprehensive tests for the Helios logging system.
//
#include <helios/core/logging.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

// ---- Test channels ----
HELIOS_DEFINE_LOG_CHANNEL(TestChannel);
HELIOS_DEFINE_LOG_CHANNEL(Renderer);
HELIOS_DEFINE_LOG_CHANNEL(Physics);
HELIOS_DEFINE_LOG_CHANNEL(Audio);
HELIOS_DEFINE_LOG_CHANNEL_WITH_LEVEL(VerboseChannel, helios::LogLevel::Debug);

// ---- Helper: read file contents ----
static std::string read_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return "";
    return std::string(std::istreambuf_iterator<char>(file),
                       std::istreambuf_iterator<char>());
}

// ---- Test fixture ----
class LoggingTest : public ::testing::Test {
protected:
    static constexpr const char* kTestLogDir = "/tmp/helios_log_test";

    void SetUp() override {
        // Clean up any previous test logs
        if (fs::exists(kTestLogDir)) {
            fs::remove_all(kTestLogDir);
        }
    }

    void TearDown() override {
        // Clean up test logs
        if (fs::exists(kTestLogDir)) {
            fs::remove_all(kTestLogDir);
        }
    }

    helios::LogConfig make_test_config() {
        helios::LogConfig config;
        config.log_directory = kTestLogDir;
        config.log_file_name = "test.log";
        config.enable_console_sink = false; // Don't pollute test output
        config.enable_file_sink = true;
        config.enable_ring_buffer_sink = true;
        config.ring_buffer_capacity = 64;
        config.max_file_size = 1024 * 1024; // 1 MB
        config.max_rotated_files = 2;
        config.default_level = helios::LogLevel::Trace;
        return config;
    }
};

// ---- Tests ----

TEST_F(LoggingTest, ConstructionSetsGlobalInstance) {
    ASSERT_EQ(helios::LogSystem::instance(), nullptr);

    {
        helios::LogSystem log_system(make_test_config());
        ASSERT_NE(helios::LogSystem::instance(), nullptr);
        EXPECT_EQ(helios::LogSystem::instance(), &log_system);
    }

    // After destruction, global instance should be null
    EXPECT_EQ(helios::LogSystem::instance(), nullptr);
}

TEST_F(LoggingTest, MoveConstructionTransfersOwnership) {
    helios::LogSystem log_system(make_test_config());
    auto* original_ptr = &log_system;
    ASSERT_EQ(helios::LogSystem::instance(), original_ptr);

    helios::LogSystem moved(std::move(log_system));
    EXPECT_EQ(helios::LogSystem::instance(), &moved);
}

TEST_F(LoggingTest, MoveAssignmentTransfersOwnership) {
    helios::LogSystem log_system(make_test_config());

    auto config2 = make_test_config();
    config2.log_file_name = "test2.log";
    helios::LogSystem log_system2(config2);

    // log_system2 is now the global
    EXPECT_EQ(helios::LogSystem::instance(), &log_system2);

    log_system2 = std::move(log_system);
    EXPECT_EQ(helios::LogSystem::instance(), &log_system2);
}

TEST_F(LoggingTest, BasicLoggingToFile) {
    helios::LogSystem log_system(make_test_config());

    HELIOS_LOG(TestChannel, Info, "Hello from test: {}", 42);
    log_system.flush();

    std::string log_path = std::string(kTestLogDir) + "/test.log";
    std::string contents = read_file(log_path);
    EXPECT_NE(contents.find("Hello from test: 42"), std::string::npos);
    EXPECT_NE(contents.find("TestChannel"), std::string::npos);
}

TEST_F(LoggingTest, AllLogLevels) {
    helios::LogSystem log_system(make_test_config());

    HELIOS_LOG(TestChannel, Trace, "trace msg");
    HELIOS_LOG(TestChannel, Debug, "debug msg");
    HELIOS_LOG(TestChannel, Info,  "info msg");
    HELIOS_LOG(TestChannel, Warn,  "warn msg");
    HELIOS_LOG(TestChannel, Error, "error msg");
    // Note: we don't test Fatal here because it calls std::abort()

    log_system.flush();

    std::string log_path = std::string(kTestLogDir) + "/test.log";
    std::string contents = read_file(log_path);

    EXPECT_NE(contents.find("trace msg"), std::string::npos);
    EXPECT_NE(contents.find("debug msg"), std::string::npos);
    EXPECT_NE(contents.find("info msg"),  std::string::npos);
    EXPECT_NE(contents.find("warn msg"),  std::string::npos);
    EXPECT_NE(contents.find("error msg"), std::string::npos);
}

TEST_F(LoggingTest, PerLevelMacros) {
    helios::LogSystem log_system(make_test_config());

    HELIOS_LOG_TRACE(TestChannel, "trace level");
    HELIOS_LOG_DEBUG(TestChannel, "debug level");
    HELIOS_LOG_INFO(TestChannel,  "info level");
    HELIOS_LOG_WARN(TestChannel,  "warn level");
    HELIOS_LOG_ERROR(TestChannel, "error level");

    log_system.flush();

    std::string log_path = std::string(kTestLogDir) + "/test.log";
    std::string contents = read_file(log_path);

    // In Debug builds (which tests use), all levels should be present
    // In Release, Trace and Debug would be stripped at compile time
    EXPECT_NE(contents.find("info level"),  std::string::npos);
    EXPECT_NE(contents.find("warn level"),  std::string::npos);
    EXPECT_NE(contents.find("error level"), std::string::npos);
}

TEST_F(LoggingTest, StructuredFieldsInOutput) {
    helios::LogSystem log_system(make_test_config());

    HELIOS_LOG(TestChannel, Info, "Loaded texture: {}", "grass.png")
        .field("width", int64_t{1024})
        .field("height", int64_t{1024})
        .field("format", "BC7");

    log_system.flush();

    std::string log_path = std::string(kTestLogDir) + "/test.log";
    std::string contents = read_file(log_path);

    EXPECT_NE(contents.find("Loaded texture: grass.png"), std::string::npos);
    EXPECT_NE(contents.find("width=1024"), std::string::npos);
    EXPECT_NE(contents.find("height=1024"), std::string::npos);
    EXPECT_NE(contents.find("format=BC7"), std::string::npos);
    // Fields should be separated by pipe
    EXPECT_NE(contents.find("|"), std::string::npos);
}

TEST_F(LoggingTest, StructuredFieldsBoolAndDouble) {
    helios::LogSystem log_system(make_test_config());

    HELIOS_LOG(TestChannel, Info, "Config loaded")
        .field("vsync", true)
        .field("fullscreen", false)
        .field("gamma", 2.2);

    log_system.flush();

    std::string log_path = std::string(kTestLogDir) + "/test.log";
    std::string contents = read_file(log_path);

    EXPECT_NE(contents.find("vsync=true"), std::string::npos);
    EXPECT_NE(contents.find("fullscreen=false"), std::string::npos);
    EXPECT_NE(contents.find("gamma="), std::string::npos);
}

TEST_F(LoggingTest, MultipleChannels) {
    helios::LogSystem log_system(make_test_config());

    HELIOS_LOG(Renderer, Info, "Renderer initialized");
    HELIOS_LOG(Physics,  Info, "Physics world created");
    HELIOS_LOG(Audio,    Info, "Audio device opened");

    log_system.flush();

    std::string log_path = std::string(kTestLogDir) + "/test.log";
    std::string contents = read_file(log_path);

    EXPECT_NE(contents.find("Renderer"), std::string::npos);
    EXPECT_NE(contents.find("Physics"),  std::string::npos);
    EXPECT_NE(contents.find("Audio"),    std::string::npos);
}

TEST_F(LoggingTest, ChannelRuntimeFiltering) {
    helios::LogSystem log_system(make_test_config());

    // Disable Physics channel
    LogChannel_Physics.set_enabled(false);

    HELIOS_LOG(Renderer, Info, "renderer visible");
    HELIOS_LOG(Physics,  Info, "physics hidden");
    HELIOS_LOG(Audio,    Info, "audio visible");

    log_system.flush();

    std::string log_path = std::string(kTestLogDir) + "/test.log";
    std::string contents = read_file(log_path);

    EXPECT_NE(contents.find("renderer visible"), std::string::npos);
    EXPECT_EQ(contents.find("physics hidden"), std::string::npos); // Should NOT appear
    EXPECT_NE(contents.find("audio visible"), std::string::npos);

    // Re-enable for other tests
    LogChannel_Physics.set_enabled(true);
}

TEST_F(LoggingTest, ChannelMinLevelFiltering) {
    helios::LogSystem log_system(make_test_config());

    // Set Renderer channel to only show Warn and above
    LogChannel_Renderer.set_level(helios::LogLevel::Warn);

    HELIOS_LOG(Renderer, Info,  "renderer info hidden");
    HELIOS_LOG(Renderer, Warn,  "renderer warn visible");
    HELIOS_LOG(Renderer, Error, "renderer error visible");

    log_system.flush();

    std::string log_path = std::string(kTestLogDir) + "/test.log";
    std::string contents = read_file(log_path);

    EXPECT_EQ(contents.find("renderer info hidden"), std::string::npos);
    EXPECT_NE(contents.find("renderer warn visible"), std::string::npos);
    EXPECT_NE(contents.find("renderer error visible"), std::string::npos);

    // Reset for other tests
    LogChannel_Renderer.set_level(helios::LogLevel::Trace);
}

TEST_F(LoggingTest, GlobalLevelFiltering) {
    helios::LogSystem log_system(make_test_config());

    log_system.set_global_level(helios::LogLevel::Error);

    HELIOS_LOG(TestChannel, Info,  "info hidden");
    HELIOS_LOG(TestChannel, Warn,  "warn hidden");
    HELIOS_LOG(TestChannel, Error, "error visible");

    log_system.flush();

    std::string log_path = std::string(kTestLogDir) + "/test.log";
    std::string contents = read_file(log_path);

    EXPECT_EQ(contents.find("info hidden"), std::string::npos);
    EXPECT_EQ(contents.find("warn hidden"), std::string::npos);
    EXPECT_NE(contents.find("error visible"), std::string::npos);
}

TEST_F(LoggingTest, RingBufferCapturesMessages) {
    helios::LogSystem log_system(make_test_config());

    for (int i = 0; i < 10; ++i) {
        HELIOS_LOG(TestChannel, Info, "ring buffer msg {}", i);
    }
    log_system.flush();

    auto messages = log_system.get_recent_messages(0);
    EXPECT_EQ(messages.size(), 10u);

    // Check that messages are in order
    for (int i = 0; i < 10; ++i) {
        std::string expected = "ring buffer msg " + std::to_string(i);
        bool found = false;
        for (const auto& msg : messages) {
            if (msg.find(expected) != std::string::npos) {
                found = true;
                break;
            }
        }
        EXPECT_TRUE(found) << "Missing message: " << expected;
    }
}

TEST_F(LoggingTest, RingBufferRespectCapacity) {
    auto config = make_test_config();
    config.ring_buffer_capacity = 5;
    helios::LogSystem log_system(config);

    for (int i = 0; i < 20; ++i) {
        HELIOS_LOG(TestChannel, Info, "overflow msg {}", i);
    }
    log_system.flush();

    auto messages = log_system.get_recent_messages(0);
    // Ring buffer should only keep the last 5
    EXPECT_LE(messages.size(), 5u);

    // The most recent messages should be present
    bool found_last = false;
    for (const auto& msg : messages) {
        if (msg.find("overflow msg 19") != std::string::npos) {
            found_last = true;
            break;
        }
    }
    EXPECT_TRUE(found_last);
}

TEST_F(LoggingTest, RingBufferPartialRetrieval) {
    helios::LogSystem log_system(make_test_config());

    for (int i = 0; i < 10; ++i) {
        HELIOS_LOG(TestChannel, Info, "partial msg {}", i);
    }
    log_system.flush();

    auto messages = log_system.get_recent_messages(3);
    EXPECT_EQ(messages.size(), 3u);
}

TEST_F(LoggingTest, CrashContextDump) {
    helios::LogSystem log_system(make_test_config());

    HELIOS_LOG(TestChannel, Info,  "before crash 1");
    HELIOS_LOG(TestChannel, Warn,  "before crash 2");
    HELIOS_LOG(TestChannel, Error, "before crash 3");
    log_system.flush();

    std::string crash_path = std::string(kTestLogDir) + "/test_crash_context.log";
    log_system.dump_crash_context(crash_path);

    std::string contents = read_file(crash_path);
    EXPECT_NE(contents.find("HELIOS CRASH CONTEXT"), std::string::npos);
    EXPECT_NE(contents.find("before crash 1"), std::string::npos);
    EXPECT_NE(contents.find("before crash 2"), std::string::npos);
    EXPECT_NE(contents.find("before crash 3"), std::string::npos);
}

TEST_F(LoggingTest, FormatStringWithMultipleArgs) {
    helios::LogSystem log_system(make_test_config());

    HELIOS_LOG(TestChannel, Info, "pos=({}, {}, {}), vel={:.2f}", 1, 2, 3, 4.567);
    log_system.flush();

    std::string log_path = std::string(kTestLogDir) + "/test.log";
    std::string contents = read_file(log_path);

    EXPECT_NE(contents.find("pos=(1, 2, 3), vel=4.57"), std::string::npos);
}

TEST_F(LoggingTest, ThreadSafety) {
    helios::LogSystem log_system(make_test_config());

    constexpr int kThreadCount = 8;
    constexpr int kMessagesPerThread = 100;

    std::vector<std::thread> threads;
    threads.reserve(kThreadCount);

    for (int t = 0; t < kThreadCount; ++t) {
        threads.emplace_back([t]() {
            for (int i = 0; i < kMessagesPerThread; ++i) {
                HELIOS_LOG(TestChannel, Info, "thread {} msg {}", t, i);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    log_system.flush();

    // Verify all messages made it to the ring buffer or file
    std::string log_path = std::string(kTestLogDir) + "/test.log";
    std::string contents = read_file(log_path);

    // We should have kThreadCount * kMessagesPerThread lines
    // Just verify some representative messages
    EXPECT_NE(contents.find("thread 0 msg 0"), std::string::npos);
    EXPECT_NE(contents.find("thread 7 msg 99"), std::string::npos);
}

TEST_F(LoggingTest, AsyncMode) {
    auto config = make_test_config();
    config.async_mode = true;
    config.async_queue_size = 4096;
    config.async_thread_count = 1;
    helios::LogSystem log_system(config);

    EXPECT_TRUE(log_system.is_async());

    for (int i = 0; i < 50; ++i) {
        HELIOS_LOG(TestChannel, Info, "async msg {}", i);
    }

    // Give async thread time to process
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    log_system.flush();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    std::string log_path = std::string(kTestLogDir) + "/test.log";
    std::string contents = read_file(log_path);

    EXPECT_NE(contents.find("async msg 0"), std::string::npos);
    EXPECT_NE(contents.find("async msg 49"), std::string::npos);
}

TEST_F(LoggingTest, NoSystemInstanceReturnsNullEntry) {
    // No LogSystem is active
    ASSERT_EQ(helios::LogSystem::instance(), nullptr);

    // This should not crash -- returns a LogEntry with null logger
    HELIOS_LOG(TestChannel, Info, "no system active");
    // If we got here without crashing, the test passes
}

TEST_F(LoggingTest, BuiltInChannels) {
    helios::LogSystem log_system(make_test_config());

    // Core and App channels are always defined
    HELIOS_LOG(Core, Info, "core channel works");
    HELIOS_LOG(App,  Info, "app channel works");
    log_system.flush();

    std::string log_path = std::string(kTestLogDir) + "/test.log";
    std::string contents = read_file(log_path);

    EXPECT_NE(contents.find("core channel works"), std::string::npos);
    EXPECT_NE(contents.find("app channel works"),  std::string::npos);
}

TEST_F(LoggingTest, ChannelWithDefaultLevel) {
    helios::LogSystem log_system(make_test_config());

    // VerboseChannel was defined with LogLevel::Debug as default
    HELIOS_LOG(VerboseChannel, Trace, "trace filtered by channel default");
    HELIOS_LOG(VerboseChannel, Debug, "debug passes channel default");
    log_system.flush();

    std::string log_path = std::string(kTestLogDir) + "/test.log";
    std::string contents = read_file(log_path);

    // Trace should be filtered by channel's default level of Debug
    EXPECT_EQ(contents.find("trace filtered by channel default"), std::string::npos);
    EXPECT_NE(contents.find("debug passes channel default"), std::string::npos);
}

TEST_F(LoggingTest, LogLevelStringConversion) {
    using helios::LogLevel;
    using helios::log_level_to_string;
    using helios::log_level_from_string;

    EXPECT_EQ(log_level_to_string(LogLevel::Trace), "Trace");
    EXPECT_EQ(log_level_to_string(LogLevel::Debug), "Debug");
    EXPECT_EQ(log_level_to_string(LogLevel::Info),  "Info");
    EXPECT_EQ(log_level_to_string(LogLevel::Warn),  "Warn");
    EXPECT_EQ(log_level_to_string(LogLevel::Error), "Error");
    EXPECT_EQ(log_level_to_string(LogLevel::Fatal), "Fatal");
    EXPECT_EQ(log_level_to_string(LogLevel::Off),   "Off");

    EXPECT_EQ(log_level_from_string("Trace"), LogLevel::Trace);
    EXPECT_EQ(log_level_from_string("debug"), LogLevel::Debug);
    EXPECT_EQ(log_level_from_string("Info"),  LogLevel::Info);
    EXPECT_EQ(log_level_from_string("warn"),  LogLevel::Warn);
    EXPECT_EQ(log_level_from_string("Error"), LogLevel::Error);
    EXPECT_EQ(log_level_from_string("fatal"), LogLevel::Fatal);
    EXPECT_EQ(log_level_from_string("Off"),   LogLevel::Off);
    EXPECT_EQ(log_level_from_string("garbage"), LogLevel::Trace); // default
}

TEST_F(LoggingTest, FlushWorks) {
    helios::LogSystem log_system(make_test_config());

    HELIOS_LOG(TestChannel, Info, "pre-flush message");
    log_system.flush();

    std::string log_path = std::string(kTestLogDir) + "/test.log";
    std::string contents = read_file(log_path);
    EXPECT_NE(contents.find("pre-flush message"), std::string::npos);
}

TEST_F(LoggingTest, TimestampAndThreadIdInOutput) {
    auto config = make_test_config();
    // Use a pattern that includes timestamp and thread ID
    config.file_pattern = "[%Y-%m-%d %H:%M:%S.%e] [tid %t] [%n] [%l] %v";
    helios::LogSystem log_system(config);

    HELIOS_LOG(TestChannel, Info, "timestamp test");
    log_system.flush();

    std::string log_path = std::string(kTestLogDir) + "/test.log";
    std::string contents = read_file(log_path);

    // Should contain a timestamp pattern like [2026-04-07 ...]
    EXPECT_NE(contents.find("[202"), std::string::npos); // Year prefix
    EXPECT_NE(contents.find("[tid"), std::string::npos); // Thread ID
}

TEST_F(LoggingTest, DisabledRingBufferReturnsEmpty) {
    auto config = make_test_config();
    config.enable_ring_buffer_sink = false;
    helios::LogSystem log_system(config);

    HELIOS_LOG(TestChannel, Info, "no ring buffer");
    log_system.flush();

    auto messages = log_system.get_recent_messages(0);
    EXPECT_TRUE(messages.empty());
}

TEST_F(LoggingTest, DisabledFileSinkNoFile) {
    auto config = make_test_config();
    config.enable_file_sink = false;
    config.enable_console_sink = false;
    // Only ring buffer
    helios::LogSystem log_system(config);

    HELIOS_LOG(TestChannel, Info, "ring only");
    log_system.flush();

    std::string log_path = std::string(kTestLogDir) + "/test.log";
    EXPECT_FALSE(fs::exists(log_path));

    // But ring buffer should have it
    auto messages = log_system.get_recent_messages(0);
    EXPECT_GE(messages.size(), 1u);
}

TEST_F(LoggingTest, SetChannelLevelViaSystem) {
    helios::LogSystem log_system(make_test_config());

    // Use the system API to set channel level
    helios::LogSystem::set_channel_level(LogChannel_TestChannel, helios::LogLevel::Error);

    HELIOS_LOG(TestChannel, Info, "filtered by system api");
    HELIOS_LOG(TestChannel, Error, "passes system api filter");
    log_system.flush();

    std::string log_path = std::string(kTestLogDir) + "/test.log";
    std::string contents = read_file(log_path);

    EXPECT_EQ(contents.find("filtered by system api"), std::string::npos);
    EXPECT_NE(contents.find("passes system api filter"), std::string::npos);

    // Reset
    helios::LogSystem::set_channel_level(LogChannel_TestChannel, helios::LogLevel::Trace);
}

TEST_F(LoggingTest, SetChannelEnabledViaSystem) {
    helios::LogSystem log_system(make_test_config());

    helios::LogSystem::set_channel_enabled(LogChannel_TestChannel, false);
    HELIOS_LOG(TestChannel, Error, "channel disabled");

    helios::LogSystem::set_channel_enabled(LogChannel_TestChannel, true);
    HELIOS_LOG(TestChannel, Info, "channel re-enabled");

    log_system.flush();

    std::string log_path = std::string(kTestLogDir) + "/test.log";
    std::string contents = read_file(log_path);

    EXPECT_EQ(contents.find("channel disabled"), std::string::npos);
    EXPECT_NE(contents.find("channel re-enabled"), std::string::npos);
}

TEST_F(LoggingTest, LogDirectoryCreated) {
    std::string nested_dir = std::string(kTestLogDir) + "/nested/deep";
    ASSERT_FALSE(fs::exists(nested_dir));

    auto config = make_test_config();
    config.log_directory = nested_dir;
    helios::LogSystem log_system(config);

    HELIOS_LOG(TestChannel, Info, "nested dir test");
    log_system.flush();

    EXPECT_TRUE(fs::exists(nested_dir));
}

TEST_F(LoggingTest, NullLogEntryFieldsCompile) {
    // NullLogEntry should accept .field() calls and do nothing
    helios::NullLogEntry null_entry;
    null_entry.field("key", "value");
    null_entry.field("num", int64_t{42});
    null_entry.field("flag", true);
    null_entry.field("pi", 3.14);
    null_entry.field("big", uint64_t{999});

    // Rvalue chain
    helios::NullLogEntry{}
        .field("a", "b")
        .field("c", int64_t{1})
        .field("d", true);

    // If we got here, NullLogEntry API is correct
}

// ============================================================================
// Standalone primitive tests (power layer)
// ============================================================================

TEST_F(LoggingTest, ConsoleSinkStandalone) {
    // ConsoleSink can be created and used without LogSystem
    ASSERT_EQ(helios::LogSystem::instance(), nullptr);

    auto console_sink = std::make_shared<helios::ConsoleSink>();
    // Writing should not crash even with no LogSystem active
    console_sink->write(helios::LogLevel::Info, "StandaloneTest", "console sink standalone write");
    console_sink->flush();
    // If we got here without crashing, standalone ConsoleSink works
}

TEST_F(LoggingTest, RingBufferSinkStandalone) {
    // RingBufferSink can be created and queried without LogSystem
    ASSERT_EQ(helios::LogSystem::instance(), nullptr);

    auto ring_sink = std::make_shared<helios::RingBufferSink>(32);

    ring_sink->write(helios::LogLevel::Info, "RingTest", "message one");
    ring_sink->write(helios::LogLevel::Warn, "RingTest", "message two");
    ring_sink->write(helios::LogLevel::Error, "RingTest", "message three");

    auto messages = ring_sink->get_messages(0);
    EXPECT_EQ(messages.size(), 3u);

    // Verify content is present
    bool found_one = false, found_two = false, found_three = false;
    for (const auto& msg : messages) {
        if (msg.find("message one") != std::string::npos) found_one = true;
        if (msg.find("message two") != std::string::npos) found_two = true;
        if (msg.find("message three") != std::string::npos) found_three = true;
    }
    EXPECT_TRUE(found_one);
    EXPECT_TRUE(found_two);
    EXPECT_TRUE(found_three);
}

TEST_F(LoggingTest, RingBufferSinkStandalonePartialRetrieval) {
    auto ring_sink = std::make_shared<helios::RingBufferSink>(32);

    for (int i = 0; i < 10; ++i) {
        ring_sink->write(helios::LogLevel::Info, "Ring", "msg " + std::to_string(i));
    }

    auto messages = ring_sink->get_messages(3);
    EXPECT_EQ(messages.size(), 3u);
}

TEST_F(LoggingTest, FileSinkStandalone) {
    // FileSink can be created and used without LogSystem
    ASSERT_EQ(helios::LogSystem::instance(), nullptr);

    std::string file_path = std::string(kTestLogDir) + "/standalone_file.log";
    auto file_sink = std::make_shared<helios::FileSink>(file_path);

    file_sink->write(helios::LogLevel::Info, "FileTest", "standalone file write");
    file_sink->flush();

    std::string contents = read_file(file_path);
    EXPECT_NE(contents.find("standalone file write"), std::string::npos);
    EXPECT_NE(contents.find("FileTest"), std::string::npos);
}

TEST_F(LoggingTest, LogChannelStandaloneWithSinks) {
    // A LogChannel with custom sinks can log without LogSystem
    ASSERT_EQ(helios::LogSystem::instance(), nullptr);

    auto ring_sink = std::make_shared<helios::RingBufferSink>(64);
    helios::LogChannel my_channel("MyModule", helios::LogLevel::Debug, {
        std::static_pointer_cast<helios::LogSink>(ring_sink)
    });

    my_channel.log(helios::LogLevel::Info, "Hello from {}", "standalone channel");
    my_channel.log(helios::LogLevel::Warn, "Warning: {}", 42);

    auto messages = ring_sink->get_messages(0);
    EXPECT_EQ(messages.size(), 2u);

    bool found_hello = false, found_warning = false;
    for (const auto& msg : messages) {
        if (msg.find("Hello from standalone channel") != std::string::npos) found_hello = true;
        if (msg.find("Warning: 42") != std::string::npos) found_warning = true;
    }
    EXPECT_TRUE(found_hello);
    EXPECT_TRUE(found_warning);
}

TEST_F(LoggingTest, LogChannelStandaloneLevelFiltering) {
    auto ring_sink = std::make_shared<helios::RingBufferSink>(64);
    helios::LogChannel ch("Filtered", helios::LogLevel::Warn, {
        std::static_pointer_cast<helios::LogSink>(ring_sink)
    });

    ch.log(helios::LogLevel::Debug, "should be filtered");
    ch.log(helios::LogLevel::Info,  "should be filtered too");
    ch.log(helios::LogLevel::Warn,  "should pass");
    ch.log(helios::LogLevel::Error, "should also pass");

    auto messages = ring_sink->get_messages(0);
    EXPECT_EQ(messages.size(), 2u);

    bool found_warn = false, found_error = false;
    for (const auto& msg : messages) {
        if (msg.find("should pass") != std::string::npos) found_warn = true;
        if (msg.find("should also pass") != std::string::npos) found_error = true;
    }
    EXPECT_TRUE(found_warn);
    EXPECT_TRUE(found_error);
}

TEST_F(LoggingTest, LogChannelStandaloneMultipleSinks) {
    auto ring_sink = std::make_shared<helios::RingBufferSink>(64);
    std::string file_path = std::string(kTestLogDir) + "/multi_sink.log";
    auto file_sink = std::make_shared<helios::FileSink>(file_path);

    helios::LogChannel ch("Multi", helios::LogLevel::Trace, {
        std::static_pointer_cast<helios::LogSink>(ring_sink),
        std::static_pointer_cast<helios::LogSink>(file_sink)
    });

    ch.log(helios::LogLevel::Info, "multi-sink message");
    ch.flush();

    // Check ring buffer
    auto messages = ring_sink->get_messages(0);
    EXPECT_GE(messages.size(), 1u);
    bool found_ring = false;
    for (const auto& msg : messages) {
        if (msg.find("multi-sink message") != std::string::npos) found_ring = true;
    }
    EXPECT_TRUE(found_ring);

    // Check file
    std::string contents = read_file(file_path);
    EXPECT_NE(contents.find("multi-sink message"), std::string::npos);
}

TEST_F(LoggingTest, LogChannelStandaloneDisable) {
    auto ring_sink = std::make_shared<helios::RingBufferSink>(64);
    helios::LogChannel ch("Disable", helios::LogLevel::Trace, {
        std::static_pointer_cast<helios::LogSink>(ring_sink)
    });

    ch.log(helios::LogLevel::Info, "before disable");
    ch.set_enabled(false);
    ch.log(helios::LogLevel::Info, "while disabled");
    ch.set_enabled(true);
    ch.log(helios::LogLevel::Info, "after re-enable");

    auto messages = ring_sink->get_messages(0);
    EXPECT_EQ(messages.size(), 2u);

    bool found_before = false, found_after = false, found_disabled = false;
    for (const auto& msg : messages) {
        if (msg.find("before disable") != std::string::npos) found_before = true;
        if (msg.find("while disabled") != std::string::npos) found_disabled = true;
        if (msg.find("after re-enable") != std::string::npos) found_after = true;
    }
    EXPECT_TRUE(found_before);
    EXPECT_FALSE(found_disabled);
    EXPECT_TRUE(found_after);
}

TEST_F(LoggingTest, LogEntryWithStandaloneChannel) {
    // LogEntry can target a standalone channel directly
    ASSERT_EQ(helios::LogSystem::instance(), nullptr);

    auto ring_sink = std::make_shared<helios::RingBufferSink>(64);
    helios::LogChannel ch("EntryTest", helios::LogLevel::Trace, {
        std::static_pointer_cast<helios::LogSink>(ring_sink)
    });

    // Create a LogEntry targeting the standalone channel
    {
        helios::LogEntry entry(&ch, helios::LogLevel::Info, "entry with fields");
        entry.field("key1", "value1");
        entry.field("count", int64_t{99});
    } // entry emits on destruction

    auto messages = ring_sink->get_messages(0);
    EXPECT_EQ(messages.size(), 1u);
    ASSERT_FALSE(messages.empty());
    EXPECT_NE(messages[0].find("entry with fields"), std::string::npos);
    EXPECT_NE(messages[0].find("key1=value1"), std::string::npos);
    EXPECT_NE(messages[0].find("count=99"), std::string::npos);
}

TEST_F(LoggingTest, LogChannelAddSinkAtRuntime) {
    helios::LogChannel ch("Dynamic", helios::LogLevel::Trace, {});
    EXPECT_FALSE(ch.has_sinks());

    auto ring_sink = std::make_shared<helios::RingBufferSink>(64);
    ch.add_sink(ring_sink);
    EXPECT_TRUE(ch.has_sinks());

    ch.log(helios::LogLevel::Info, "after add_sink");

    auto messages = ring_sink->get_messages(0);
    EXPECT_EQ(messages.size(), 1u);
}

TEST_F(LoggingTest, CustomSinkImplementation) {
    // Users can implement their own LogSink
    struct CountingSink : helios::LogSink {
        int count = 0;
        std::string last_message;

        void write(helios::LogLevel /*level*/, const std::string& /*channel_name*/,
                   const std::string& message) override {
            ++count;
            last_message = message;
        }
        void flush() override {}
    };

    auto custom_sink = std::make_shared<CountingSink>();
    helios::LogChannel ch("Custom", helios::LogLevel::Trace, {custom_sink});

    ch.log(helios::LogLevel::Info, "first");
    ch.log(helios::LogLevel::Info, "second");
    ch.log(helios::LogLevel::Info, "third");

    EXPECT_EQ(custom_sink->count, 3);
    EXPECT_EQ(custom_sink->last_message, "third");
}

TEST_F(LoggingTest, StandaloneDoesNotInterfereWithMacros) {
    // Standalone channels and LogSystem can coexist
    auto ring_standalone = std::make_shared<helios::RingBufferSink>(64);
    helios::LogChannel standalone_ch("Standalone", helios::LogLevel::Trace, {
        std::static_pointer_cast<helios::LogSink>(ring_standalone)
    });

    // Create LogSystem for macro usage
    helios::LogSystem log_system(make_test_config());

    // Log through both paths
    standalone_ch.log(helios::LogLevel::Info, "standalone msg");
    HELIOS_LOG(TestChannel, Info, "macro msg");
    log_system.flush();

    // Standalone ring buffer should only have the standalone message
    auto standalone_msgs = ring_standalone->get_messages(0);
    EXPECT_EQ(standalone_msgs.size(), 1u);
    ASSERT_FALSE(standalone_msgs.empty());
    EXPECT_NE(standalone_msgs[0].find("standalone msg"), std::string::npos);

    // LogSystem ring buffer should only have the macro message
    auto system_msgs = log_system.get_recent_messages(0);
    bool found_macro = false, found_standalone = false;
    for (const auto& msg : system_msgs) {
        if (msg.find("macro msg") != std::string::npos) found_macro = true;
        if (msg.find("standalone msg") != std::string::npos) found_standalone = true;
    }
    EXPECT_TRUE(found_macro);
    EXPECT_FALSE(found_standalone);
}
