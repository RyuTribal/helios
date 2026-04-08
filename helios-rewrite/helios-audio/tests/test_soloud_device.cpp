// helios-audio/tests/test_soloud_device.cpp

#include <gtest/gtest.h>
#include "soloud/soloud_device.h"

using namespace helios::audio;

class SoLoudDeviceTest : public ::testing::Test {
protected:
    void SetUp() override {
        device = std::make_unique<SoLoudDevice>();
    }

    void TearDown() override {
        device.reset();
    }

    std::unique_ptr<SoLoudDevice> device;
};

TEST_F(SoLoudDeviceTest, ConstructionAndDestruction) {
    // RAII: constructor inits SoLoud, destructor deinits. No crash = pass.
    EXPECT_NE(device, nullptr);
}

TEST_F(SoLoudDeviceTest, PlayNullDataReturnsZero) {
    auto handle = device->play(nullptr, 0);
    EXPECT_EQ(handle, 0u);
}

TEST_F(SoLoudDeviceTest, StopInvalidHandleNoOp) {
    device->stop(999);
    // No crash = pass.
}

TEST_F(SoLoudDeviceTest, IsPlayingInvalidHandle) {
    EXPECT_FALSE(device->is_playing(999));
}

TEST_F(SoLoudDeviceTest, SetListenerNoOp) {
    device->set_listener({0, 0, 0}, {0, 0, -1}, {0, 1, 0});
    // No crash = pass.
}

TEST_F(SoLoudDeviceTest, UpdateNoOp) {
    device->update();
    // No crash = pass.
}
