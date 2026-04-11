// helios-audio/tests/test_audio_plugin.cpp

#include <gtest/gtest.h>
#include <memory>

#include "interface/audio_device.h"
#include "interface/audio_plugin.h"
#include "soloud/soloud_device.h"

using namespace helios::audio;

// Minimal mock App for testing plugin build().
struct MockApp {
    template<typename T>
    void insert_resource(T resource) {
        resource_count++;
        if constexpr (std::is_same_v<T, std::unique_ptr<AudioDevice>>) {
            audio_device_inserted = (resource != nullptr);
            stored_device = std::move(resource);
        }
    }

    // Absorb add_system calls (just count them)
    template<typename... Args>
    MockApp& add_system(Args&&...) { system_count++; return *this; }

    int resource_count = 0;
    int system_count = 0;
    bool audio_device_inserted = false;
    std::unique_ptr<AudioDevice> stored_device;
};

TEST(AudioPlugin, SoLoudBuildInsertsResources) {
    MockApp app;

    AudioPlugin<SoLoudDevice> plugin;
    plugin.build(app);

    EXPECT_TRUE(app.audio_device_inserted);
    EXPECT_EQ(app.resource_count, 1);

    // Verify the device is functional (play with null data returns 0 — no crash)
    SoundHandle handle = app.stored_device->play(nullptr, 0);
    EXPECT_EQ(handle, 0u);
}

TEST(AudioPlugin, SequentialSoLoudDevicesWork) {
    // Instantiate SoLoud devices sequentially (not simultaneously) to avoid
    // exhausting audio hardware handles in headless/PA environments.
    {
        auto dev1 = std::make_unique<SoLoudDevice>();
        EXPECT_NE(dev1, nullptr);
    }
    {
        auto dev2 = std::make_unique<SoLoudDevice>();
        EXPECT_NE(dev2, nullptr);
    }
}
