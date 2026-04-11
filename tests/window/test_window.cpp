// tests/window/test_window.cpp
// Window system tests.
// Tests that require a real GLFW window skip on headless CI.
#include <gtest/gtest.h>
#include "helios/window/window_types.h"
#include "helios/window/window.h"
#include "helios/window/windows.h"

using namespace helios;

// ---------------------------------------------------------------------------
// Headless-safe: WindowDesc defaults and WindowId types
// ---------------------------------------------------------------------------

TEST(WindowDescTest, DefaultValues) {
    WindowDesc desc;
    EXPECT_EQ(desc.title, "Helios");
    EXPECT_EQ(desc.width, 1280u);
    EXPECT_EQ(desc.height, 720u);
    EXPECT_TRUE(desc.vsync);
    EXPECT_FALSE(desc.fullscreen);
    EXPECT_TRUE(desc.resizable);
}

TEST(WindowDescTest, CustomValues) {
    WindowDesc desc{
        .title      = "TestWindow",
        .width      = 800,
        .height     = 600,
        .vsync      = false,
        .fullscreen = true,
        .resizable  = false,
    };
    EXPECT_EQ(desc.title, "TestWindow");
    EXPECT_EQ(desc.width, 800u);
    EXPECT_EQ(desc.height, 600u);
    EXPECT_FALSE(desc.vsync);
    EXPECT_TRUE(desc.fullscreen);
    EXPECT_FALSE(desc.resizable);
}

TEST(WindowIdTest, InvalidWindowIdIsZero) {
    EXPECT_EQ(InvalidWindowId, 0u);
}

TEST(WindowIdTest, WindowIdIsUint32) {
    static_assert(std::is_same_v<WindowId, uint32_t>, "WindowId must be uint32_t");
    SUCCEED();
}

TEST(WindowIdTest, IncrementingIds) {
    // Verify that WindowId can hold typical sequential values.
    WindowId id1 = 1;
    WindowId id2 = 2;
    EXPECT_LT(id1, id2);
    EXPECT_NE(id1, InvalidWindowId);
    EXPECT_NE(id2, InvalidWindowId);
}

// ---------------------------------------------------------------------------
// Tests requiring a display server (skip on headless CI)
// ---------------------------------------------------------------------------

static bool has_display() {
    return std::getenv("DISPLAY") != nullptr ||
           std::getenv("WAYLAND_DISPLAY") != nullptr;
}

class WindowDisplayTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!has_display()) {
            GTEST_SKIP() << "No display server available, skipping window tests";
        }
    }
};

TEST_F(WindowDisplayTest, CreateAndDestroy) {
    WindowDesc desc{ .title = "Test", .width = 320, .height = 240 };
    {
        Window win(desc);
        // Note: WMs / compositors may override requested size, so only check
        // that the window was created with a positive size.
        EXPECT_GT(win.width(), 0u);
        EXPECT_GT(win.height(), 0u);
        EXPECT_FALSE(win.should_close());
        EXPECT_NE(win.native_handle(), nullptr);
        EXPECT_EQ(win.title(), "Test");
    }
    // Destructor should have run without crash.
}

TEST_F(WindowDisplayTest, MoveConstructor) {
    WindowDesc desc{ .title = "MoveTest", .width = 640, .height = 480 };
    Window a(desc);
    void* handle = a.native_handle();

    Window b(std::move(a));
    // After move, b should own the same native handle.
    EXPECT_EQ(b.native_handle(), handle);
    // WMs may override size, just verify the window is valid.
    EXPECT_GT(b.width(), 0u);
}

TEST_F(WindowDisplayTest, MoveAssignment) {
    WindowDesc desc_a{ .title = "A", .width = 100, .height = 100 };
    WindowDesc desc_b{ .title = "B", .width = 200, .height = 200 };

    Window a(desc_a);
    Window b(desc_b);

    b = std::move(a);
    EXPECT_EQ(b.title(), "A");
}

TEST_F(WindowDisplayTest, WindowsManagerHeadlessMode) {
    // Default-constructed Windows has no primary (headless mode).
    Windows mgr;
    EXPECT_FALSE(mgr.has_primary());
    EXPECT_EQ(mgr.count(), 0u);
}

TEST_F(WindowDisplayTest, WindowsManagerCreateDestroy) {
    Windows mgr(Window(WindowDesc{ .title = "Primary", .width = 800, .height = 600 }));

    EXPECT_TRUE(mgr.has_primary());
    EXPECT_EQ(mgr.count(), 1u);
    EXPECT_EQ(mgr.primary().title(), "Primary");

    WindowId second = mgr.create(WindowDesc{ .title = "Second", .width = 400, .height = 300 });
    EXPECT_EQ(mgr.count(), 2u);
    EXPECT_EQ(mgr.get(second).title(), "Second");

    mgr.destroy(second);
    EXPECT_EQ(mgr.count(), 1u);
}

TEST_F(WindowDisplayTest, PollEventsDoesNotCrash) {
    WindowDesc desc{ .title = "PollTest", .width = 320, .height = 240 };
    Window win(desc);
    // poll_events is tested by poll_all via Windows; the window itself
    // exposes callback_data which should start clean.
    const auto& cb = win.callback_data();
    EXPECT_FALSE(cb.resized);
    EXPECT_FALSE(cb.close_requested);
    EXPECT_TRUE(cb.key_events.empty());
}
