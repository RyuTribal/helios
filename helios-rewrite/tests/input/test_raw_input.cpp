// tests/input/test_raw_input.cpp
// RawInput state tracking tests (headless-safe, no GLFW needed).
#include <gtest/gtest.h>
#include "helios/input/raw_input.h"

using namespace helios;

TEST(RawInputTest, InitialStateAllUnpressed) {
    RawInput input;
    EXPECT_FALSE(input.key_pressed(KeyCode::A));
    EXPECT_FALSE(input.key_just_pressed(KeyCode::A));
    EXPECT_FALSE(input.key_just_released(KeyCode::A));
    EXPECT_FALSE(input.mouse_button_pressed(MouseButton::Left));
    EXPECT_FLOAT_EQ(input.scroll_delta(), 0.0f);
    EXPECT_EQ(input.mouse_position(), glm::vec2(0.0f, 0.0f));
    EXPECT_EQ(input.mouse_delta(), glm::vec2(0.0f, 0.0f));
}

TEST(RawInputTest, KeyPressedAndJustPressed) {
    RawInput input;

    // Frame 1: begin_frame snapshots previous (all false), then key A is pressed.
    input.begin_frame();
    input.m_keys_current[static_cast<int>(KeyCode::A)] = true;

    EXPECT_TRUE(input.key_pressed(KeyCode::A));
    EXPECT_TRUE(input.key_just_pressed(KeyCode::A));   // current=true, previous=false
    EXPECT_FALSE(input.key_just_released(KeyCode::A));

    // Frame 2: begin_frame snapshots previous (A=true), key A still held.
    input.begin_frame();
    input.m_keys_current[static_cast<int>(KeyCode::A)] = true;

    EXPECT_TRUE(input.key_pressed(KeyCode::A));
    EXPECT_FALSE(input.key_just_pressed(KeyCode::A));  // current=true, previous=true
    EXPECT_FALSE(input.key_just_released(KeyCode::A));
}

TEST(RawInputTest, KeyJustReleased) {
    RawInput input;

    // Frame 1: press A.
    input.begin_frame();
    input.m_keys_current[static_cast<int>(KeyCode::A)] = true;

    // Frame 2: release A — begin_frame snapshots previous=true, then set current=false.
    input.begin_frame();
    input.m_keys_current[static_cast<int>(KeyCode::A)] = false;

    EXPECT_FALSE(input.key_pressed(KeyCode::A));
    EXPECT_FALSE(input.key_just_pressed(KeyCode::A));
    EXPECT_TRUE(input.key_just_released(KeyCode::A));  // previous=true, current=false
}

TEST(RawInputTest, KeyHeldIsNotJustPressed) {
    RawInput input;

    // Frame 1: press.
    input.begin_frame();
    input.m_keys_current[static_cast<int>(KeyCode::Space)] = true;
    EXPECT_TRUE(input.key_just_pressed(KeyCode::Space));

    // Frame 2: still held.
    input.begin_frame();
    input.m_keys_current[static_cast<int>(KeyCode::Space)] = true;
    EXPECT_FALSE(input.key_just_pressed(KeyCode::Space));
    EXPECT_TRUE(input.key_pressed(KeyCode::Space));

    // Frame 3: released.
    input.begin_frame();
    input.m_keys_current[static_cast<int>(KeyCode::Space)] = false;
    EXPECT_TRUE(input.key_just_released(KeyCode::Space));
    EXPECT_FALSE(input.key_pressed(KeyCode::Space));

    // Frame 4: still up — just_released is false.
    input.begin_frame();
    EXPECT_FALSE(input.key_just_released(KeyCode::Space));
}

TEST(RawInputTest, MouseButtonJustPressed) {
    RawInput input;

    // Frame 1: click left.
    input.begin_frame();
    input.m_mouse_buttons_current[static_cast<int>(MouseButton::Left)] = true;

    EXPECT_TRUE(input.mouse_button_pressed(MouseButton::Left));
    EXPECT_TRUE(input.mouse_button_just_pressed(MouseButton::Left));
    EXPECT_FALSE(input.mouse_button_just_released(MouseButton::Left));

    // Frame 2: release left.
    input.begin_frame();
    input.m_mouse_buttons_current[static_cast<int>(MouseButton::Left)] = false;

    EXPECT_FALSE(input.mouse_button_pressed(MouseButton::Left));
    EXPECT_TRUE(input.mouse_button_just_released(MouseButton::Left));
    EXPECT_FALSE(input.mouse_button_just_pressed(MouseButton::Left));
}

TEST(RawInputTest, MousePositionAndDelta) {
    RawInput input;

    // Frame 1: mouse starts at origin, set to (100, 200).
    input.begin_frame();
    input.m_mouse_pos = glm::vec2(100.0f, 200.0f);

    EXPECT_EQ(input.mouse_position(), glm::vec2(100.0f, 200.0f));
    // Previous was (0,0), delta = current - prev = (100, 200).
    EXPECT_EQ(input.mouse_delta(), glm::vec2(100.0f, 200.0f));

    // Frame 2: begin_frame snaps prev to (100,200), then move to (150, 220).
    input.begin_frame();
    input.m_mouse_pos = glm::vec2(150.0f, 220.0f);

    EXPECT_EQ(input.mouse_position(), glm::vec2(150.0f, 220.0f));
    EXPECT_EQ(input.mouse_delta(), glm::vec2(50.0f, 20.0f));
}

TEST(RawInputTest, MouseDeltaIsZeroWhenNotMoved) {
    RawInput input;
    input.m_mouse_pos = glm::vec2(50.0f, 50.0f);

    // Frame 1: snap prev to (50,50), mouse still at (50,50).
    input.begin_frame();
    input.m_mouse_pos = glm::vec2(50.0f, 50.0f);

    EXPECT_EQ(input.mouse_delta(), glm::vec2(0.0f, 0.0f));
}

TEST(RawInputTest, ScrollDeltaResetsPerFrame) {
    RawInput input;

    // Frame 1: scroll +3.
    input.begin_frame();
    input.m_scroll = 3.0f;
    EXPECT_FLOAT_EQ(input.scroll_delta(), 3.0f);

    // Frame 2: begin_frame resets scroll to 0, no new scroll.
    input.begin_frame();
    EXPECT_FLOAT_EQ(input.scroll_delta(), 0.0f);
}

TEST(RawInputTest, ScrollAccumulatesWithinFrame) {
    RawInput input;

    input.begin_frame();
    input.m_scroll = 5.0f;
    EXPECT_FLOAT_EQ(input.scroll_delta(), 5.0f);

    // Adding more scroll in same frame.
    input.m_scroll += 2.0f;
    EXPECT_FLOAT_EQ(input.scroll_delta(), 7.0f);
}

TEST(RawInputTest, OutOfRangeKeyCodeReturnsFalse) {
    RawInput input;
    // KeyCode::Unknown = -1, should not crash or return true.
    EXPECT_FALSE(input.key_pressed(KeyCode::Unknown));
    EXPECT_FALSE(input.key_just_pressed(KeyCode::Unknown));
    EXPECT_FALSE(input.key_just_released(KeyCode::Unknown));
}

TEST(RawInputTest, BeginFrameResetsPreviousState) {
    RawInput input;

    // Press A in frame 1.
    input.begin_frame();
    input.m_keys_current[static_cast<int>(KeyCode::A)] = true;

    // Begin frame 2: previous should now be true.
    input.begin_frame();
    // After begin_frame, previous = old current (A=true), current is still true
    // (begin_frame copies current to previous, doesn't reset current).
    EXPECT_EQ(input.m_keys_previous[static_cast<int>(KeyCode::A)], true);
}

TEST(RawInputTest, GamepadStubReturnsFalse) {
    RawInput input;
    EXPECT_FLOAT_EQ(input.gamepad_axis(0, GamepadAxis::LeftX), 0.0f);
    EXPECT_FALSE(input.gamepad_button(0, GamepadButton::A));
}
