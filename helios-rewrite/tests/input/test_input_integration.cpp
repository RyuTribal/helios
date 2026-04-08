// tests/input/test_input_integration.cpp
// Integration tests for RawInput + InputMap working together.
// The pure in-memory tests (direct struct manipulation) are always run.
// Tests that require WindowPlugin (and a real GLFW window) skip on headless CI.
#include <gtest/gtest.h>
#include "helios/input/raw_input.h"
#include "helios/input/input_map.h"
#include "helios/input/input_systems.h"
#include "helios/window/window_plugin.h"
#include "helios/window/window_events.h"
#include "helios/window/windows.h"
#include "helios/input/input_plugin.h"
#include "helios/ecs/app.h"

using namespace helios;

static bool has_display() {
    return std::getenv("DISPLAY") != nullptr ||
           std::getenv("WAYLAND_DISPLAY") != nullptr;
}

// ---------------------------------------------------------------------------
// Pure in-memory integration: RawInput feeds InputMap directly.
// No GLFW, no display required.
// ---------------------------------------------------------------------------

class RawInputMapIntegration : public ::testing::Test {
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

    void next_frame() {
        raw.begin_frame();
    }
};

TEST_F(RawInputMapIntegration, InputMapReflectsFakeKeyPress) {
    map.action("jump", KeyCode::Space);
    map.axis("horizontal", KeyCode::D, KeyCode::A);

    next_frame();
    EXPECT_FALSE(map.pressed("jump"));
    EXPECT_FLOAT_EQ(map.axis_value("horizontal"), 0.0f);

    press_key(KeyCode::Space);
    EXPECT_TRUE(map.pressed("jump"));
    EXPECT_TRUE(map.just_pressed("jump"));
    EXPECT_FLOAT_EQ(map.axis_value("horizontal"), 0.0f);
}

TEST_F(RawInputMapIntegration, AxisWorksAfterKeyPressReleaseCycle) {
    map.axis("horizontal", KeyCode::D, KeyCode::A);

    // Frame 1: press D.
    next_frame();
    press_key(KeyCode::D);
    EXPECT_FLOAT_EQ(map.axis_value("horizontal"), 1.0f);

    // Frame 2: also press A (cancel out).
    next_frame();
    press_key(KeyCode::D);
    press_key(KeyCode::A);
    EXPECT_FLOAT_EQ(map.axis_value("horizontal"), 0.0f);

    // Frame 3: release D, only A held.
    next_frame();
    release_key(KeyCode::D);
    press_key(KeyCode::A);
    EXPECT_FLOAT_EQ(map.axis_value("horizontal"), -1.0f);

    // Frame 4: release all keys, then advance frame.
    release_key(KeyCode::A);
    next_frame();
    EXPECT_FLOAT_EQ(map.axis_value("horizontal"), 0.0f);
}

TEST_F(RawInputMapIntegration, MultipleActionsIndependent) {
    map.action("jump",  KeyCode::Space);
    map.action("shoot", KeyCode::LeftControl);
    map.action("dash",  MouseButton::Right);

    next_frame();
    press_key(KeyCode::Space);
    raw.m_mouse_buttons_current[static_cast<int>(MouseButton::Right)] = true;

    EXPECT_TRUE(map.pressed("jump"));
    EXPECT_FALSE(map.pressed("shoot"));
    EXPECT_TRUE(map.pressed("dash"));
}

TEST_F(RawInputMapIntegration, JustPressedJustReleasedMultiFrame) {
    map.action("fire", KeyCode::F);

    // Frame 1: press.
    next_frame();
    press_key(KeyCode::F);
    EXPECT_TRUE(map.just_pressed("fire"));
    EXPECT_FALSE(map.just_released("fire"));

    // Frame 2: held — not just pressed.
    next_frame();
    press_key(KeyCode::F);
    EXPECT_FALSE(map.just_pressed("fire"));
    EXPECT_FALSE(map.just_released("fire"));
    EXPECT_TRUE(map.pressed("fire"));

    // Frame 3: released.
    next_frame();
    release_key(KeyCode::F);
    EXPECT_FALSE(map.pressed("fire"));
    EXPECT_FALSE(map.just_pressed("fire"));
    EXPECT_TRUE(map.just_released("fire"));

    // Frame 4: still released.
    next_frame();
    EXPECT_FALSE(map.pressed("fire"));
    EXPECT_FALSE(map.just_released("fire"));
}

TEST_F(RawInputMapIntegration, UnregisteredActionReturnsFalse) {
    // Map has no actions bound.
    next_frame();
    press_key(KeyCode::A);
    EXPECT_FALSE(map.pressed("undefined_action"));
    EXPECT_FLOAT_EQ(map.axis_value("undefined_axis"), 0.0f);
}

// ---------------------------------------------------------------------------
// Plugin integration: requires a display.
// ---------------------------------------------------------------------------

class PluginIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!has_display()) {
            GTEST_SKIP() << "No display server available, skipping plugin integration tests";
        }
    }
};

TEST_F(PluginIntegrationTest, WindowPluginInsertsWindowsResource) {
    App app;
    app.add_plugin(WindowPlugin{
        .primary_window = { .title = "IntegrationTest", .width = 320, .height = 240 }
    });

    World& world = app.world();
    ASSERT_TRUE(world.has_resource<Windows>());
    EXPECT_TRUE(world.resource<Windows>().has_primary());
    EXPECT_EQ(world.resource<Windows>().primary().title(), "IntegrationTest");
}

TEST_F(PluginIntegrationTest, InputPluginInsertsRawInputAndInputMap) {
    App app;
    app.add_plugin(WindowPlugin{
        .primary_window = { .title = "InputTest", .width = 320, .height = 240 }
    });
    app.add_plugin(InputPlugin{});

    World& world = app.world();
    ASSERT_TRUE(world.has_resource<RawInput>());
    ASSERT_TRUE(world.has_resource<InputMap>());
}

TEST_F(PluginIntegrationTest, TickRunsInputSystemsWithoutCrash) {
    App app;
    app.add_plugin(WindowPlugin{
        .primary_window = { .title = "TickTest", .width = 320, .height = 240 }
    });
    app.add_plugin(InputPlugin{});

    // Configure some actions.
    auto& input_map = app.world().resource<InputMap>();
    input_map.action("jump", KeyCode::Space);
    input_map.axis("horizontal", KeyCode::D, KeyCode::A);

    // Tick runs PreUpdate (poll_window_events, update_raw_input, update_action_map).
    EXPECT_NO_THROW(app.tick());

    // With no events, RawInput should be in its default state.
    const auto& raw = app.world().resource<RawInput>();
    EXPECT_FALSE(raw.key_pressed(KeyCode::Space));
    EXPECT_EQ(raw.mouse_position(), glm::vec2(0.0f, 0.0f));
    EXPECT_FLOAT_EQ(raw.scroll_delta(), 0.0f);
}

TEST_F(PluginIntegrationTest, InputMapWiredAfterTick) {
    // After app.tick(), update_action_map has run and wired the InputMap to RawInput.
    // Manually injecting a key into RawInput should be reflected in InputMap.
    App app;
    app.add_plugin(WindowPlugin{
        .primary_window = { .title = "WireTest", .width = 320, .height = 240 }
    });
    app.add_plugin(InputPlugin{});

    auto& input_map = app.world().resource<InputMap>();
    input_map.action("jump", KeyCode::Space);

    app.tick();  // Wires InputMap to RawInput via update_action_map.

    // Manually set a key in RawInput to simulate input.
    auto& raw = app.world().resource<RawInput>();
    raw.m_keys_current[static_cast<int>(KeyCode::Space)] = true;

    // InputMap should reflect the injected state.
    EXPECT_TRUE(input_map.pressed("jump"));
}

TEST_F(PluginIntegrationTest, WindowResizedEventCanBeEmitted) {
    App app;
    app.add_plugin(WindowPlugin{
        .primary_window = { .title = "EventTest", .width = 320, .height = 240 }
    });

    World& world = app.world();

    // WindowPlugin registers WindowResized events.
    // Events are double-buffered: send() writes to the write buffer.
    // swap_event_buffers() moves them to the read buffer (happens each tick).
    auto writer = world.event_writer<WindowResized>();
    writer.send(WindowResized{ .window_id = 1, .width = 640, .height = 480 });

    // Simulate the end-of-frame swap so the reader can see the events.
    world.swap_event_buffers();

    auto reader = world.event_reader<WindowResized>();
    int count = 0;
    for (const auto& ev : reader) {
        EXPECT_EQ(ev.width, 640u);
        EXPECT_EQ(ev.height, 480u);
        count++;
    }
    EXPECT_EQ(count, 1);
}
