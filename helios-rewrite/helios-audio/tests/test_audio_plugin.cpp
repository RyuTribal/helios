// helios-audio/tests/test_audio_plugin.cpp

#include <gtest/gtest.h>
#include <memory>

#include "interface/audio_device.h"
#include "interface/audio_plugin.h"
#include "stub/stub_audio_device.h"

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

    int resource_count = 0;
    bool audio_device_inserted = false;
    std::unique_ptr<AudioDevice> stored_device;
};

TEST(AudioPlugin, StubBuildInsertsResources) {
    MockApp app;

    AudioPlugin<StubAudioDevice> plugin;
    plugin.build(app);

    EXPECT_TRUE(app.audio_device_inserted);
    EXPECT_EQ(app.resource_count, 1);

    // Verify the device is functional
    std::vector<uint8_t> fake_wav(128, 0);
    SoundHandle handle = app.stored_device->play(fake_wav.data(), fake_wav.size());
    EXPECT_NE(handle, 0u);
    app.stored_device->stop(handle);
}

TEST(AudioPlugin, MultipleStubDevicesWork) {
    auto dev1 = std::make_unique<StubAudioDevice>();
    auto dev2 = std::make_unique<StubAudioDevice>();

    std::vector<uint8_t> fake_wav(128, 0);
    SoundHandle h1 = dev1->play(fake_wav.data(), fake_wav.size());
    SoundHandle h2 = dev2->play(fake_wav.data(), fake_wav.size());

    EXPECT_NE(h1, 0u);
    EXPECT_NE(h2, 0u);

    dev1.reset();
    dev2.reset();
}
