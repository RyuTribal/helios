// tests/input/test_input_map.cpp
// InputMap action/axis binding and query tests (headless-safe, no GLFW needed).
#include <gtest/gtest.h>
#include "helios/input/input_map.h"
#include "helios/input/raw_input.h"

using namespace helios;

class InputMapTest : public ::testing::Test {
protected:
    RawInput raw;
    InputMap map;

    void SetUp() override {
        map.set_raw_input(&raw);
    }

    void press_key(KeyCode key) {
        raw.m_keys_current[static_cast<int>(key)] = true;
    }

    void release_key(KeyCode key) {
        raw.m_keys_current[static_cast<int>(key)] = false;
    }

    void press_mouse(MouseButton btn) {
        raw.m_mouse_buttons_current[static_cast<int>(btn)] = true;
    }

    void release_mouse(MouseButton btn) {
        raw.m_mouse_buttons_current[static_cast<int>(btn)] = false;
    }

    void next_frame() {
        raw.begin_frame();
    }
};

TEST_F(InputMapTest, SingleKeyAction) {
    map.action("jump", KeyCode::Space);

    next_frame();
    EXPECT_FALSE(map.pressed("jump"));
    EXPECT_FALSE(map.just_pressed("jump"));

    press_key(KeyCode::Space);
    EXPECT_TRUE(map.pressed("jump"));
    EXPECT_TRUE(map.just_pressed("jump"));  // current=true, previous=false

    next_frame();
    press_key(KeyCode::Space);  // still held
    EXPECT_TRUE(map.pressed("jump"));
    EXPECT_FALSE(map.just_pressed("jump")); // held, not just pressed
}

TEST_F(InputMapTest, MultipleBindingsSameAction) {
    map.action("fire", KeyCode::Space);
    map.action("fire", MouseButton::Left);

    next_frame();
    EXPECT_FALSE(map.pressed("fire"));

    // Keyboard binding activates action.
    press_key(KeyCode::Space);
    EXPECT_TRUE(map.pressed("fire"));

    // After releasing keyboard, mouse activates action.
    next_frame();
    release_key(KeyCode::Space);
    press_mouse(MouseButton::Left);
    EXPECT_TRUE(map.pressed("fire"));
}

TEST_F(InputMapTest, ActionJustReleased) {
    map.action("crouch", KeyCode::LeftControl);

    next_frame();
    press_key(KeyCode::LeftControl);
    EXPECT_TRUE(map.just_pressed("crouch"));

    next_frame();
    release_key(KeyCode::LeftControl);
    EXPECT_TRUE(map.just_released("crouch"));
    EXPECT_FALSE(map.pressed("crouch"));

    next_frame();
    EXPECT_FALSE(map.just_released("crouch")); // only true for one frame
}

TEST_F(InputMapTest, UnknownActionReturnsFalse) {
    EXPECT_FALSE(map.pressed("nonexistent"));
    EXPECT_FALSE(map.just_pressed("nonexistent"));
    EXPECT_FALSE(map.just_released("nonexistent"));
    EXPECT_FLOAT_EQ(map.axis_value("nonexistent"), 0.0f);
}

TEST_F(InputMapTest, KeyboardAxis) {
    map.axis("horizontal", KeyCode::D, KeyCode::A);

    next_frame();
    EXPECT_FLOAT_EQ(map.axis_value("horizontal"), 0.0f);

    // Press positive key (+1).
    press_key(KeyCode::D);
    EXPECT_FLOAT_EQ(map.axis_value("horizontal"), 1.0f);

    // Press both keys (cancel out → 0).
    press_key(KeyCode::A);
    EXPECT_FLOAT_EQ(map.axis_value("horizontal"), 0.0f);

    // Release positive key, only negative held (→ -1).
    release_key(KeyCode::D);
    EXPECT_FLOAT_EQ(map.axis_value("horizontal"), -1.0f);
}

TEST_F(InputMapTest, AxisValueIsZeroWhenNoKeysPressed) {
    map.axis("vertical", KeyCode::W, KeyCode::S);

    next_frame();
    EXPECT_FLOAT_EQ(map.axis_value("vertical"), 0.0f);
}

TEST_F(InputMapTest, NoRawInputReturnsFalseAndZero) {
    // InputMap without set_raw_input (m_raw stays null).
    InputMap detached;
    detached.action("test", KeyCode::A);
    detached.axis("h", KeyCode::D, KeyCode::A);

    EXPECT_FALSE(detached.pressed("test"));
    EXPECT_FALSE(detached.just_pressed("test"));
    EXPECT_FALSE(detached.just_released("test"));
    EXPECT_FLOAT_EQ(detached.axis_value("h"), 0.0f);
}

TEST_F(InputMapTest, ChainingBindings) {
    // Builder pattern chaining should compile and work.
    map.action("shoot", KeyCode::Space)
       .action("shoot", MouseButton::Left)
       .axis("move_x", KeyCode::D, KeyCode::A)
       .axis("move_y", KeyCode::W, KeyCode::S);

    next_frame();
    press_key(KeyCode::W);
    EXPECT_FLOAT_EQ(map.axis_value("move_y"), 1.0f);
    EXPECT_FLOAT_EQ(map.axis_value("move_x"), 0.0f);
    EXPECT_FALSE(map.pressed("shoot"));
}

TEST_F(InputMapTest, MouseButtonJustPressed) {
    map.action("interact", MouseButton::Right);

    next_frame();
    EXPECT_FALSE(map.pressed("interact"));

    press_mouse(MouseButton::Right);
    EXPECT_TRUE(map.pressed("interact"));
    EXPECT_TRUE(map.just_pressed("interact"));

    next_frame();
    release_mouse(MouseButton::Right);
    EXPECT_FALSE(map.pressed("interact"));
    EXPECT_TRUE(map.just_released("interact"));
}

TEST_F(InputMapTest, MultipleAxesIndependent) {
    map.axis("h", KeyCode::D, KeyCode::A);
    map.axis("v", KeyCode::W, KeyCode::S);

    next_frame();
    press_key(KeyCode::D);
    press_key(KeyCode::S);

    EXPECT_FLOAT_EQ(map.axis_value("h"),  1.0f);
    EXPECT_FLOAT_EQ(map.axis_value("v"), -1.0f);
}
