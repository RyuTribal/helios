// helios-rewrite/tests/core/test_log_channel_registry.cpp
//
// Tests for LogChannelRegistry: duplicate detection, find, all, deregistration.
//
#include <helios/core/logging.h>

#include <gtest/gtest.h>

#include <string>

// ============================================================================
// Registry tests
// ============================================================================

TEST(LogChannelRegistryTest, RegisteredChannelIsFindableByName) {
    // Create a standalone channel with a unique name.
    auto ring = std::make_shared<helios::RingBufferSink>(8);
    helios::LogChannel ch("RegistryFindTest", helios::LogLevel::Trace, {
        std::static_pointer_cast<helios::LogSink>(ring)
    });

    helios::LogChannel* found = helios::LogChannelRegistry::find("RegistryFindTest");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found, &ch);
    EXPECT_STREQ(found->name, "RegistryFindTest");
}

TEST(LogChannelRegistryTest, FindReturnsNullptrForUnknownName) {
    EXPECT_EQ(helios::LogChannelRegistry::find("NonExistentChannel_xyz_42"), nullptr);
}

TEST(LogChannelRegistryTest, AllReturnsRegisteredChannels) {
    auto before = helios::LogChannelRegistry::all();

    auto ring = std::make_shared<helios::RingBufferSink>(8);
    helios::LogChannel ch("RegistryAllTest", helios::LogLevel::Trace, {
        std::static_pointer_cast<helios::LogSink>(ring)
    });

    auto after = helios::LogChannelRegistry::all();
    EXPECT_EQ(after.size(), before.size() + 1);

    // The new channel must be in the list.
    bool found = false;
    for (auto* c : after) {
        if (c == &ch) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

TEST(LogChannelRegistryTest, DuplicateNameAborts) {
    // Creating two different LogChannel objects with the same name must abort.
    EXPECT_DEATH(
        {
            helios::LogChannel ch1("DuplicateDeathTest", helios::LogLevel::Trace);
            helios::LogChannel ch2("DuplicateDeathTest", helios::LogLevel::Trace);
            // Should never reach here.
            (void)ch1;
            (void)ch2;
        },
        "Duplicate log channel name \"DuplicateDeathTest\"");
}

TEST(LogChannelRegistryTest, DestroyedChannelIsRemovedFromRegistry) {
    {
        auto ring = std::make_shared<helios::RingBufferSink>(8);
        helios::LogChannel ch("RegistryDestroyTest", helios::LogLevel::Trace, {
            std::static_pointer_cast<helios::LogSink>(ring)
        });

        // While alive, it should be findable.
        ASSERT_NE(helios::LogChannelRegistry::find("RegistryDestroyTest"), nullptr);
    }

    // After destruction, it should be gone.
    EXPECT_EQ(helios::LogChannelRegistry::find("RegistryDestroyTest"), nullptr);
}

TEST(LogChannelRegistryTest, InlineGlobalChannelsAreRegistered) {
    // The built-in channels (Core, App) defined via HELIOS_DEFINE_LOG_CHANNEL
    // in log_macros.h should be registered at static init time.
    EXPECT_NE(helios::LogChannelRegistry::find("Core"), nullptr);
    EXPECT_NE(helios::LogChannelRegistry::find("App"),  nullptr);
}

TEST(LogChannelRegistryTest, NameReuseAfterDestructionSucceeds) {
    // Destroying a channel should free up its name for reuse.
    {
        helios::LogChannel ch1("ReuseTest", helios::LogLevel::Trace);
        ASSERT_NE(helios::LogChannelRegistry::find("ReuseTest"), nullptr);
    }
    // ch1 is destroyed; name is free.
    {
        helios::LogChannel ch2("ReuseTest", helios::LogLevel::Trace);
        EXPECT_NE(helios::LogChannelRegistry::find("ReuseTest"), nullptr);
    }
}
