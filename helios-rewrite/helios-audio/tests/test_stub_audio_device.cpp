// helios-audio/tests/test_stub_audio_device.cpp

#include <gtest/gtest.h>
#include <vector>

#include "interface/audio_device.h"
#include "stub/stub_audio_device.h"

using namespace helios::audio;

class StubAudioDeviceTest : public ::testing::Test {
protected:
    void SetUp() override {
        device = std::make_unique<StubAudioDevice>();
    }

    void TearDown() override {
        device.reset();
    }

    // Generate a minimal fake WAV buffer (content does not matter for stub)
    static std::vector<uint8_t> generate_test_wav() {
        return std::vector<uint8_t>(128, 0);
    }

    std::unique_ptr<StubAudioDevice> device;
};

// --- Construction / destruction ---

TEST_F(StubAudioDeviceTest, ConstructionAndDestructionDoNotCrash) {
    EXPECT_NE(device, nullptr);
}

// --- Play / stop ---

TEST_F(StubAudioDeviceTest, PlayReturnsNonZeroHandle) {
    auto wav_data = generate_test_wav();
    SoundHandle handle = device->play(wav_data.data(), wav_data.size());
    EXPECT_NE(handle, 0u);
    device->stop(handle);
}

TEST_F(StubAudioDeviceTest, StopInvalidHandleDoesNotCrash) {
    EXPECT_NO_THROW(device->stop(999));
}

TEST_F(StubAudioDeviceTest, IsPlayingReturnsTrueForActiveSound) {
    auto wav_data = generate_test_wav();
    SoundHandle handle = device->play(wav_data.data(), wav_data.size());
    EXPECT_TRUE(device->is_playing(handle));
    device->stop(handle);
}

TEST_F(StubAudioDeviceTest, IsPlayingReturnsFalseAfterStop) {
    auto wav_data = generate_test_wav();
    SoundHandle handle = device->play(wav_data.data(), wav_data.size());
    device->stop(handle);
    EXPECT_FALSE(device->is_playing(handle));
}

// --- Pause / resume ---

TEST_F(StubAudioDeviceTest, PauseAndResumeDoNotCrash) {
    auto wav_data = generate_test_wav();
    SoundHandle handle = device->play(wav_data.data(), wav_data.size());
    EXPECT_NO_THROW(device->pause(handle));
    // While paused, is_playing should return false (paused != playing)
    EXPECT_FALSE(device->is_playing(handle));
    EXPECT_NO_THROW(device->resume(handle));
    EXPECT_TRUE(device->is_playing(handle));
    device->stop(handle);
}

// --- 3D audio ---

TEST_F(StubAudioDeviceTest, PlayAtCreatesSound) {
    auto wav_data = generate_test_wav();
    SoundHandle handle = device->play_at(
        wav_data.data(), wav_data.size(),
        glm::vec3(1.0f, 2.0f, 3.0f)
    );
    EXPECT_NE(handle, 0u);
    EXPECT_TRUE(device->is_playing(handle));
    device->stop(handle);
}

TEST_F(StubAudioDeviceTest, SetListenerDoesNotCrash) {
    EXPECT_NO_THROW(device->set_listener(
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 0.0f, -1.0f),
        glm::vec3(0.0f, 1.0f, 0.0f)
    ));
}

TEST_F(StubAudioDeviceTest, SetPositionOnActiveSound) {
    auto wav_data = generate_test_wav();
    SoundHandle handle = device->play_at(
        wav_data.data(), wav_data.size(),
        glm::vec3(0.0f)
    );
    EXPECT_NO_THROW(device->set_position(handle, glm::vec3(10.0f, 0.0f, 0.0f)));
    device->stop(handle);
}

// --- Update ---

TEST_F(StubAudioDeviceTest, UpdateDoesNotCrash) {
    EXPECT_NO_THROW(device->update());
}

// --- Interface polymorphism ---

TEST_F(StubAudioDeviceTest, WorksThroughBasePointer) {
    std::unique_ptr<AudioDevice> base = std::make_unique<StubAudioDevice>();
    auto wav_data = generate_test_wav();

    SoundHandle handle = base->play(wav_data.data(), wav_data.size());
    EXPECT_NE(handle, 0u);
    EXPECT_TRUE(base->is_playing(handle));

    base->update();
    base->stop(handle);
    EXPECT_FALSE(base->is_playing(handle));
}
