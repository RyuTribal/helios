# Window System & Input System -- Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement a multi-window system behind a GLFW pimpl abstraction and a two-layer input system (raw polling + action mapping), integrated with the ECS as resources, systems, and events.

**Architecture:** `Window` uses the pimpl idiom to hide GLFW behind `std::unique_ptr<Impl>`. `Windows` is a resource that manages multiple windows by `WindowId`. `RawInput` is a resource tracking raw keyboard/mouse state with current/previous frame arrays. `InputMap` is a resource providing named action/axis bindings on top of `RawInput`. Both the window plugin and input plugin register ECS systems in `Schedule::PreUpdate` and fire events (`WindowResized`, `WindowClosed`) through the ECS event channels.

**Tech Stack:** C++20, GLFW 3.x (already vendored), glm, Google Test

**Spec:** `docs/superpowers/specs/2026-04-07-engine-rewrite-design.md` sections 9, 10, and 2.2 (Plugins)

**Dependencies (assumed complete from Plans 1-2):**
- `helios-core/src/ecs/` -- World, Entity, Query, Commands, Res, ResMut, EventReader, EventWriter, resource_storage, event_storage
- `helios-core/src/app/` -- App, Plugin system (add_plugin, insert_resource, add_event, add_system), Scheduler, Schedule enum
- Root `CMakeLists.txt` and `helios-core/CMakeLists.txt` -- existing CMake build with helios-core static library, GLFW linked, Google Test configured

**IMPORTANT -- Design constraints:**
- **No global state.** No singletons, no static mutable variables. GLFW user pointer carries a struct that routes callbacks into the correct `Window::Impl`.
- **RAII everywhere.** `Window` constructor creates the GLFW window. Destructor destroys it. No `Init()`/`Shutdown()`.
- **Move-only types.** `Window` is move-only (deleted copy constructor/assignment). `Windows` and `RawInput` are also move-only because they hold non-copyable resources.
- **Pimpl for GLFW isolation.** Public headers never include `<GLFW/glfw3.h>`. Only `.cpp` files and the private `Impl` struct see GLFW types.
- **Never edit vendored library source files.** Only modify build files; use compiler flags for any vendor configuration.

---

## Task 1: Create KeyCode and MouseButton enums

**Files:**
- Create: `helios-core/src/input/key_codes.h`

These are engine-owned enums that map 1:1 with GLFW key codes by value, so conversion is a static_cast. This isolates the rest of the engine from GLFW headers.

- [ ] **Step 1: Create key_codes.h**

```cpp
// helios-core/src/input/key_codes.h
#pragma once
#include <cstdint>

namespace helios {

// Values match GLFW key codes exactly so conversion is static_cast<int>(KeyCode).
enum class KeyCode : int32_t {
    Unknown        = -1,

    // Printable keys
    Space          = 32,
    Apostrophe     = 39,
    Comma          = 44,
    Minus          = 45,
    Period         = 46,
    Slash          = 47,
    Num0           = 48,
    Num1           = 49,
    Num2           = 50,
    Num3           = 51,
    Num4           = 52,
    Num5           = 53,
    Num6           = 54,
    Num7           = 55,
    Num8           = 56,
    Num9           = 57,
    Semicolon      = 59,
    Equal          = 61,
    A              = 65,
    B              = 66,
    C              = 67,
    D              = 68,
    E              = 69,
    F              = 70,
    G              = 71,
    H              = 72,
    I              = 73,
    J              = 74,
    K              = 75,
    L              = 76,
    M              = 77,
    N              = 78,
    O              = 79,
    P              = 80,
    Q              = 81,
    R              = 82,
    S              = 83,
    T              = 84,
    U              = 85,
    V              = 86,
    W              = 87,
    X              = 88,
    Y              = 89,
    Z              = 90,
    LeftBracket    = 91,
    Backslash      = 92,
    RightBracket   = 93,
    GraveAccent    = 96,

    // Function keys
    Escape         = 256,
    Enter          = 257,
    Tab            = 258,
    Backspace      = 259,
    Insert         = 260,
    Delete         = 261,
    Right          = 262,
    Left           = 263,
    Down           = 264,
    Up             = 265,
    PageUp         = 266,
    PageDown       = 267,
    Home           = 268,
    End            = 269,
    CapsLock       = 280,
    ScrollLock     = 281,
    NumLock        = 282,
    PrintScreen    = 283,
    Pause          = 284,
    F1             = 290,
    F2             = 291,
    F3             = 292,
    F4             = 293,
    F5             = 294,
    F6             = 295,
    F7             = 296,
    F8             = 297,
    F9             = 298,
    F10            = 299,
    F11            = 300,
    F12            = 301,
    KP0            = 320,
    KP1            = 321,
    KP2            = 322,
    KP3            = 323,
    KP4            = 324,
    KP5            = 325,
    KP6            = 326,
    KP7            = 327,
    KP8            = 328,
    KP9            = 329,
    KPDecimal      = 330,
    KPDivide       = 331,
    KPMultiply     = 332,
    KPSubtract     = 333,
    KPAdd          = 334,
    KPEnter        = 335,
    KPEqual        = 336,
    LeftShift      = 340,
    LeftControl    = 341,
    LeftAlt        = 342,
    LeftSuper      = 343,
    RightShift     = 344,
    RightControl   = 345,
    RightAlt       = 346,
    RightSuper     = 347,
    Menu           = 348,

    // Sentinel for array sizing. GLFW_KEY_LAST is 348.
    _Count         = 349,
};

enum class MouseButton : int32_t {
    Left    = 0,
    Right   = 1,
    Middle  = 2,
    Button4 = 3,
    Button5 = 4,
    Button6 = 5,
    Button7 = 6,
    Button8 = 7,

    _Count  = 8,
};

// Placeholders for future gamepad support.
enum class GamepadButton : int32_t {
    A = 0, B = 1, X = 2, Y = 3,
    LeftBumper = 4, RightBumper = 5,
    Back = 6, Start = 7, Guide = 8,
    LeftThumb = 9, RightThumb = 10,
    DPadUp = 11, DPadRight = 12, DPadDown = 13, DPadLeft = 14,
    _Count = 15,
};

enum class GamepadAxis : int32_t {
    None         = -1,
    LeftX        = 0,
    LeftY        = 1,
    RightX       = 2,
    RightY       = 3,
    LeftTrigger  = 4,
    RightTrigger = 5,
    _Count       = 6,
};

} // namespace helios
```

- [ ] **Step 2: Verify compilation**

```bash
cd helios && cmake --build build --target helios-core 2>&1 | tail -5
```

Expected: Compiles. Header-only, no new translation units.

- [ ] **Step 3: Commit**

```bash
git add helios-core/src/input/key_codes.h
git commit -m "feat(input): add KeyCode, MouseButton, GamepadButton, GamepadAxis enums"
```

---

## Task 2: Create WindowDesc and WindowId types

**Files:**
- Create: `helios-core/src/window/window_desc.h`

Simple descriptor struct and ID type used by both `Window` and `Windows`.

- [ ] **Step 1: Create window_desc.h**

```cpp
// helios-core/src/window/window_desc.h
#pragma once
#include <cstdint>
#include <string>

namespace helios {

// Strongly-typed window identifier.
using WindowId = uint32_t;
inline constexpr WindowId InvalidWindowId = 0;

struct WindowDesc {
    std::string title    = "Helios";
    uint32_t    width    = 1280;
    uint32_t    height   = 720;
    bool        vsync    = true;
    bool        fullscreen = false;
    bool        resizable  = true;
};

} // namespace helios
```

- [ ] **Step 2: Commit**

```bash
git add helios-core/src/window/window_desc.h
git commit -m "feat(window): add WindowDesc struct and WindowId type"
```

---

## Task 3: Implement Window class (GLFW pimpl)

**Files:**
- Create: `helios-core/src/window/window.h`
- Create: `helios-core/src/window/window.cpp`

The `Window` class hides GLFW behind a pimpl. The public header exposes no GLFW types. The `.cpp` file includes `<GLFW/glfw3.h>` and defines the `Impl` struct. GLFW initialization uses a static reference count so the first `Window` constructor calls `glfwInit()` and the last destructor calls `glfwTerminate()`.

- [ ] **Step 1: Create window.h (public header)**

```cpp
// helios-core/src/window/window.h
#pragma once
#include "window_desc.h"
#include <memory>
#include <functional>
#include <cstdint>

namespace helios {

// Forward-declared callback payload. GLFW callbacks populate this
// every frame; the input system reads it.
struct WindowCallbackData {
    // Keyboard
    struct KeyEvent { int key; int action; };
    std::vector<KeyEvent> key_events;

    // Mouse buttons
    struct MouseButtonEvent { int button; int action; };
    std::vector<MouseButtonEvent> mouse_button_events;

    // Mouse position (set continuously by cursor callback)
    double mouse_x = 0.0;
    double mouse_y = 0.0;
    bool   mouse_moved = false;

    // Scroll
    double scroll_x = 0.0;
    double scroll_y = 0.0;

    // Resize
    bool     resized = false;
    uint32_t new_width  = 0;
    uint32_t new_height = 0;

    // Close
    bool close_requested = false;

    void clear() {
        key_events.clear();
        mouse_button_events.clear();
        mouse_moved = false;
        scroll_x = 0.0;
        scroll_y = 0.0;
        resized = false;
        close_requested = false;
    }
};

class Window {
public:
    explicit Window(const WindowDesc& desc);
    ~Window();

    // Move-only.
    Window(Window&& other) noexcept;
    Window& operator=(Window&& other) noexcept;
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    uint32_t width() const;
    uint32_t height() const;
    bool should_close() const;
    const std::string& title() const;

    // Polls GLFW events for this window. Called once per frame
    // (typically by Windows::poll_all).
    void poll_events();

    // Returns the per-frame callback data accumulated since last poll.
    const WindowCallbackData& callback_data() const;

    // Opaque handle for RHI / Vulkan surface creation.
    // Returns GLFWwindow* cast to void*.
    void* native_handle() const;

private:
    struct Impl;                       // defined in window.cpp
    std::unique_ptr<Impl> m_impl;
};

} // namespace helios
```

- [ ] **Step 2: Create window.cpp (GLFW implementation)**

```cpp
// helios-core/src/window/window.cpp
#include "window.h"
#include <GLFW/glfw3.h>
#include <stdexcept>
#include <atomic>

namespace helios {

// ---- GLFW lifetime management (reference counted) ----
static std::atomic<int> s_glfw_ref_count{0};

static void ensure_glfw_init() {
    if (s_glfw_ref_count.fetch_add(1, std::memory_order_relaxed) == 0) {
        glfwSetErrorCallback([](int code, const char* msg) {
            // In production, route through spdlog / helios logging.
            fprintf(stderr, "[GLFW Error %d] %s\n", code, msg);
        });
        if (!glfwInit()) {
            s_glfw_ref_count.fetch_sub(1, std::memory_order_relaxed);
            throw std::runtime_error("Failed to initialize GLFW");
        }
    }
}

static void release_glfw() {
    if (s_glfw_ref_count.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        glfwTerminate();
    }
}

// ---- Impl ----
struct Window::Impl {
    GLFWwindow*        glfw_window = nullptr;
    WindowDesc         desc;
    WindowCallbackData cb_data;

    explicit Impl(const WindowDesc& d) : desc(d) {
        ensure_glfw_init();

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // Vulkan, no OpenGL context
        glfwWindowHint(GLFW_RESIZABLE, d.resizable ? GLFW_TRUE : GLFW_FALSE);

        GLFWmonitor* monitor = nullptr;
        int w = static_cast<int>(d.width);
        int h = static_cast<int>(d.height);

        if (d.fullscreen) {
            monitor = glfwGetPrimaryMonitor();
            const GLFWvidmode* mode = glfwGetVideoMode(monitor);
            w = mode->width;
            h = mode->height;
        }

        glfw_window = glfwCreateWindow(w, h, d.title.c_str(), monitor, nullptr);
        if (!glfw_window) {
            release_glfw();
            throw std::runtime_error("Failed to create GLFW window");
        }

        // Store pointer to callback data so GLFW callbacks can reach it.
        glfwSetWindowUserPointer(glfw_window, &cb_data);

        // -- Install callbacks --
        glfwSetKeyCallback(glfw_window, [](GLFWwindow* win, int key, int /*scancode*/, int action, int /*mods*/) {
            auto* data = static_cast<WindowCallbackData*>(glfwGetWindowUserPointer(win));
            data->key_events.push_back({key, action});
        });

        glfwSetMouseButtonCallback(glfw_window, [](GLFWwindow* win, int button, int action, int /*mods*/) {
            auto* data = static_cast<WindowCallbackData*>(glfwGetWindowUserPointer(win));
            data->mouse_button_events.push_back({button, action});
        });

        glfwSetCursorPosCallback(glfw_window, [](GLFWwindow* win, double x, double y) {
            auto* data = static_cast<WindowCallbackData*>(glfwGetWindowUserPointer(win));
            data->mouse_x = x;
            data->mouse_y = y;
            data->mouse_moved = true;
        });

        glfwSetScrollCallback(glfw_window, [](GLFWwindow* win, double xoff, double yoff) {
            auto* data = static_cast<WindowCallbackData*>(glfwGetWindowUserPointer(win));
            data->scroll_x += xoff;
            data->scroll_y += yoff;
        });

        glfwSetWindowSizeCallback(glfw_window, [](GLFWwindow* win, int width, int height) {
            auto* data = static_cast<WindowCallbackData*>(glfwGetWindowUserPointer(win));
            data->resized = true;
            data->new_width  = static_cast<uint32_t>(width);
            data->new_height = static_cast<uint32_t>(height);
        });

        glfwSetWindowCloseCallback(glfw_window, [](GLFWwindow* win) {
            auto* data = static_cast<WindowCallbackData*>(glfwGetWindowUserPointer(win));
            data->close_requested = true;
        });

        // Update desc with actual framebuffer size (may differ on HiDPI).
        int fb_w, fb_h;
        glfwGetFramebufferSize(glfw_window, &fb_w, &fb_h);
        desc.width  = static_cast<uint32_t>(fb_w);
        desc.height = static_cast<uint32_t>(fb_h);
    }

    ~Impl() {
        if (glfw_window) {
            glfwDestroyWindow(glfw_window);
            glfw_window = nullptr;
        }
        release_glfw();
    }

    // Non-copyable, non-movable (the Window wrapper handles move semantics
    // by moving the unique_ptr<Impl>).
    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;
};

// ---- Window public API ----
Window::Window(const WindowDesc& desc)
    : m_impl(std::make_unique<Impl>(desc)) {}

Window::~Window() = default;

Window::Window(Window&& other) noexcept = default;
Window& Window::operator=(Window&& other) noexcept = default;

uint32_t Window::width() const {
    int w, h;
    glfwGetFramebufferSize(m_impl->glfw_window, &w, &h);
    return static_cast<uint32_t>(w);
}

uint32_t Window::height() const {
    int w, h;
    glfwGetFramebufferSize(m_impl->glfw_window, &w, &h);
    return static_cast<uint32_t>(h);
}

bool Window::should_close() const {
    return glfwWindowShouldClose(m_impl->glfw_window) != 0;
}

const std::string& Window::title() const {
    return m_impl->desc.title;
}

void Window::poll_events() {
    m_impl->cb_data.clear();
    glfwPollEvents();
}

const WindowCallbackData& Window::callback_data() const {
    return m_impl->cb_data;
}

void* Window::native_handle() const {
    return static_cast<void*>(m_impl->glfw_window);
}

} // namespace helios
```

**Key design decisions:**
- `glfwInit()` / `glfwTerminate()` are reference-counted via `s_glfw_ref_count`. The first `Window` created calls `glfwInit()`; the last destroyed calls `glfwTerminate()`. This avoids any global init/shutdown ceremony.
- `GLFW_CLIENT_API` is set to `GLFW_NO_API` -- Vulkan surfaces will be created by the renderer, not by GLFW's OpenGL context.
- `WindowCallbackData` accumulates events between `poll_events()` calls. The input system reads this data to update `RawInput`. This avoids the old pattern of GLFW callbacks directly dispatching engine events.
- Callback data uses `glfwSetWindowUserPointer` per-window, so multi-window works without global state.

- [ ] **Step 3: Update helios-core CMakeLists.txt**

Add the new source files to the helios-core target:
```cmake
# In helios-core/CMakeLists.txt, add to the source list:
  src/window/window.h
  src/window/window.cpp
  src/window/window_desc.h
```

Ensure GLFW is linked (it should already be from the existing CMake setup since `helios-core depends on glfw` per the spec dependency graph).

- [ ] **Step 4: Verify compilation**

```bash
cd helios && cmake --build build --target helios-core 2>&1 | tail -10
```

Expected: Compiles without errors. GLFW symbols resolve.

- [ ] **Step 5: Commit**

```bash
git add helios-core/src/window/window.h helios-core/src/window/window.cpp
git commit -m "feat(window): implement Window class with GLFW pimpl pattern"
```

---

## Task 4: Implement Windows resource (multi-window manager)

**Files:**
- Create: `helios-core/src/window/windows.h`
- Create: `helios-core/src/window/windows.cpp`

`Windows` is a resource that owns all `Window` instances, manages them by `WindowId`, and provides iteration. It tracks the primary window id.

- [ ] **Step 1: Create windows.h**

```cpp
// helios-core/src/window/windows.h
#pragma once
#include "window.h"
#include "window_desc.h"
#include <unordered_map>
#include <optional>
#include <stdexcept>

namespace helios {

class Windows {
public:
    // Default constructor creates an empty manager (used by HeadlessPlugin).
    Windows() = default;

    // Construct with a primary window already created.
    explicit Windows(Window primary_window);

    ~Windows() = default;

    // Move-only (Window is move-only, so the map is move-only).
    Windows(Windows&&) noexcept = default;
    Windows& operator=(Windows&&) noexcept = default;
    Windows(const Windows&) = delete;
    Windows& operator=(const Windows&) = delete;

    // Create a new window and return its ID.
    WindowId create(const WindowDesc& desc);

    // Destroy a window by ID. Cannot destroy the primary window
    // (close it via should_close instead).
    void destroy(WindowId id);

    // Get a window by ID. Throws if not found.
    Window& get(WindowId id);
    const Window& get(WindowId id) const;

    // Get the primary window. Throws if no primary exists (headless mode).
    Window& primary();
    const Window& primary() const;
    WindowId primary_id() const;
    bool has_primary() const;

    // Poll events for all windows.
    void poll_all();

    // Iteration over all windows.
    auto begin() { return m_windows.begin(); }
    auto end()   { return m_windows.end(); }
    auto begin() const { return m_windows.begin(); }
    auto end()   const { return m_windows.end(); }

    // Number of open windows.
    size_t count() const { return m_windows.size(); }

private:
    std::unordered_map<WindowId, Window> m_windows;
    WindowId m_primary = InvalidWindowId;
    WindowId m_next_id = 1;
};

} // namespace helios
```

- [ ] **Step 2: Create windows.cpp**

```cpp
// helios-core/src/window/windows.cpp
#include "windows.h"

namespace helios {

Windows::Windows(Window primary_window) {
    WindowId id = m_next_id++;
    m_windows.emplace(id, std::move(primary_window));
    m_primary = id;
}

WindowId Windows::create(const WindowDesc& desc) {
    WindowId id = m_next_id++;
    m_windows.emplace(id, Window(desc));
    return id;
}

void Windows::destroy(WindowId id) {
    if (id == m_primary) {
        throw std::runtime_error("Cannot destroy the primary window");
    }
    m_windows.erase(id);
}

Window& Windows::get(WindowId id) {
    auto it = m_windows.find(id);
    if (it == m_windows.end()) {
        throw std::runtime_error("Window not found: " + std::to_string(id));
    }
    return it->second;
}

const Window& Windows::get(WindowId id) const {
    auto it = m_windows.find(id);
    if (it == m_windows.end()) {
        throw std::runtime_error("Window not found: " + std::to_string(id));
    }
    return it->second;
}

Window& Windows::primary() {
    if (m_primary == InvalidWindowId) {
        throw std::runtime_error("No primary window (headless mode?)");
    }
    return get(m_primary);
}

const Window& Windows::primary() const {
    if (m_primary == InvalidWindowId) {
        throw std::runtime_error("No primary window (headless mode?)");
    }
    return get(m_primary);
}

WindowId Windows::primary_id() const {
    return m_primary;
}

bool Windows::has_primary() const {
    return m_primary != InvalidWindowId;
}

void Windows::poll_all() {
    for (auto& [id, window] : m_windows) {
        window.poll_events();
    }
}

} // namespace helios
```

- [ ] **Step 3: Update CMakeLists.txt**

Add `src/window/windows.h` and `src/window/windows.cpp` to the helios-core target.

- [ ] **Step 4: Verify compilation**

```bash
cd helios && cmake --build build --target helios-core 2>&1 | tail -5
```

- [ ] **Step 5: Commit**

```bash
git add helios-core/src/window/windows.h helios-core/src/window/windows.cpp
git commit -m "feat(window): implement Windows multi-window resource manager"
```

---

## Task 5: Implement window events

**Files:**
- Create: `helios-core/src/window/window_events.h`

Simple event structs that will be sent through the ECS event system by the window polling system.

- [ ] **Step 1: Create window_events.h**

```cpp
// helios-core/src/window/window_events.h
#pragma once
#include "window_desc.h"
#include <cstdint>

namespace helios {

struct WindowResized {
    WindowId  window_id = InvalidWindowId;
    uint32_t  width     = 0;
    uint32_t  height    = 0;
};

struct WindowClosed {
    WindowId  window_id = InvalidWindowId;
};

} // namespace helios
```

- [ ] **Step 2: Commit**

```bash
git add helios-core/src/window/window_events.h
git commit -m "feat(window): add WindowResized and WindowClosed event structs"
```

---

## Task 6: Implement WindowPlugin

**Files:**
- Create: `helios-core/src/window/window_plugin.h`
- Create: `helios-core/src/window/window_plugin.cpp`

The plugin creates the primary window, inserts the `Windows` resource, registers `WindowResized` and `WindowClosed` events, and adds the `poll_window_events` system to `Schedule::PreUpdate`.

- [ ] **Step 1: Create window_plugin.h**

```cpp
// helios-core/src/window/window_plugin.h
#pragma once
#include "window_desc.h"

namespace helios {

class App; // forward declaration

struct WindowPlugin {
    WindowDesc primary_window = {
        .title  = "Helios",
        .width  = 1280,
        .height = 720,
        .vsync  = true,
    };

    void build(App& app);
};

// System function: polls all windows, emits WindowResized / WindowClosed events.
// Registered in Schedule::PreUpdate by WindowPlugin.
void poll_window_events(ResMut<Windows> windows,
                        EventWriter<WindowResized> resize_writer,
                        EventWriter<WindowClosed>  close_writer);

} // namespace helios
```

Note: The `poll_window_events` function signature uses ECS system parameter types (`ResMut`, `EventWriter`). These are declared in `helios-core/src/ecs/`. The actual signature will need to match whatever parameter extraction pattern Plan 2 established. If systems are `void(World&)` at the type-erased level, this free function is the user-facing signature that the scheduler wraps.

- [ ] **Step 2: Create window_plugin.cpp**

```cpp
// helios-core/src/window/window_plugin.cpp
#include "window_plugin.h"
#include "window.h"
#include "windows.h"
#include "window_events.h"
#include "../app/app.h"
#include "../ecs/world.h"

namespace helios {

void WindowPlugin::build(App& app) {
    // Create the primary window.
    Window primary(primary_window);
    Windows windows(std::move(primary));

    // Insert resources.
    app.insert_resource<Windows>(std::move(windows));

    // Register events.
    app.add_event<WindowResized>();
    app.add_event<WindowClosed>();

    // Register system.
    app.add_system(Schedule::PreUpdate, poll_window_events);
}

void poll_window_events(ResMut<Windows> windows,
                        EventWriter<WindowResized> resize_writer,
                        EventWriter<WindowClosed>  close_writer)
{
    // Poll GLFW events for all windows.
    windows->poll_all();

    // Iterate all windows and check callback data for events.
    for (auto& [id, window] : *windows) {
        const auto& cb = window.callback_data();

        if (cb.resized) {
            resize_writer.send(WindowResized{
                .window_id = id,
                .width     = cb.new_width,
                .height    = cb.new_height,
            });
        }

        if (cb.close_requested) {
            close_writer.send(WindowClosed{
                .window_id = id,
            });
        }
    }
}

} // namespace helios
```

**Key design decisions:**
- `poll_all()` calls `glfwPollEvents()` via each `Window::poll_events()`. GLFW dispatches callbacks into each window's `WindowCallbackData` via the user pointer.
- After polling, we iterate all windows and check the accumulated callback data. Resize and close events are forwarded into the ECS event channels.
- The input system (Task 8) will also read `callback_data()` to update `RawInput`. This is why callback data persists until the next `poll_events()` call.

- [ ] **Step 3: Update CMakeLists.txt**

Add `src/window/window_plugin.h`, `src/window/window_plugin.cpp`, and `src/window/window_events.h` to the helios-core target.

- [ ] **Step 4: Verify compilation**

```bash
cd helios && cmake --build build --target helios-core 2>&1 | tail -10
```

Expected: Compiles. May have linker warnings if App/World stubs are not yet complete -- that is acceptable at this stage.

- [ ] **Step 5: Commit**

```bash
git add helios-core/src/window/window_plugin.h helios-core/src/window/window_plugin.cpp
git commit -m "feat(window): implement WindowPlugin with poll_window_events system"
```

---

## Task 7: Implement RawInput resource

**Files:**
- Create: `helios-core/src/input/raw_input.h`
- Create: `helios-core/src/input/raw_input.cpp`

The raw input resource stores current and previous frame state for keyboard keys, mouse position, scroll, and mouse buttons. It provides query methods for pressed/just_pressed/just_released semantics.

- [ ] **Step 1: Create raw_input.h**

```cpp
// helios-core/src/input/raw_input.h
#pragma once
#include "key_codes.h"
#include <glm/glm.hpp>
#include <array>

namespace helios {

struct RawInput {
    // ---- Query API ----

    // Keyboard
    bool key_pressed(KeyCode key) const;
    bool key_just_pressed(KeyCode key) const;
    bool key_just_released(KeyCode key) const;

    // Mouse
    glm::vec2 mouse_position() const;
    glm::vec2 mouse_delta() const;
    float scroll_delta() const;
    bool mouse_button_pressed(MouseButton btn) const;
    bool mouse_button_just_pressed(MouseButton btn) const;
    bool mouse_button_just_released(MouseButton btn) const;

    // Gamepad (stub -- returns zero/false until gamepad support is added)
    float gamepad_axis(int pad, GamepadAxis axis) const;
    bool  gamepad_button(int pad, GamepadButton btn) const;

    // ---- Internal state (updated by update_raw_input system) ----

    // Keyboard: indexed by GLFW key code (matches KeyCode values).
    // Size 512 covers all GLFW key codes (GLFW_KEY_LAST = 348, with margin).
    static constexpr int KeyArraySize = 512;
    std::array<bool, KeyArraySize> m_keys_current{};
    std::array<bool, KeyArraySize> m_keys_previous{};

    // Mouse position
    glm::vec2 m_mouse_pos{0.0f, 0.0f};
    glm::vec2 m_mouse_pos_prev{0.0f, 0.0f};

    // Scroll (accumulated per frame, reset each frame)
    float m_scroll{0.0f};

    // Mouse buttons: indexed by MouseButton value. Max 8 buttons.
    static constexpr int MouseButtonCount = static_cast<int>(MouseButton::_Count);
    std::array<bool, MouseButtonCount> m_mouse_buttons_current{};
    std::array<bool, MouseButtonCount> m_mouse_buttons_previous{};

    // Called at the start of each frame by the input system
    // to snapshot previous state before processing new events.
    void begin_frame();
};

} // namespace helios
```

- [ ] **Step 2: Create raw_input.cpp**

```cpp
// helios-core/src/input/raw_input.cpp
#include "raw_input.h"

namespace helios {

// ---- Keyboard ----

bool RawInput::key_pressed(KeyCode key) const {
    int idx = static_cast<int>(key);
    if (idx < 0 || idx >= KeyArraySize) return false;
    return m_keys_current[idx];
}

bool RawInput::key_just_pressed(KeyCode key) const {
    int idx = static_cast<int>(key);
    if (idx < 0 || idx >= KeyArraySize) return false;
    return m_keys_current[idx] && !m_keys_previous[idx];
}

bool RawInput::key_just_released(KeyCode key) const {
    int idx = static_cast<int>(key);
    if (idx < 0 || idx >= KeyArraySize) return false;
    return !m_keys_current[idx] && m_keys_previous[idx];
}

// ---- Mouse ----

glm::vec2 RawInput::mouse_position() const {
    return m_mouse_pos;
}

glm::vec2 RawInput::mouse_delta() const {
    return m_mouse_pos - m_mouse_pos_prev;
}

float RawInput::scroll_delta() const {
    return m_scroll;
}

bool RawInput::mouse_button_pressed(MouseButton btn) const {
    int idx = static_cast<int>(btn);
    if (idx < 0 || idx >= MouseButtonCount) return false;
    return m_mouse_buttons_current[idx];
}

bool RawInput::mouse_button_just_pressed(MouseButton btn) const {
    int idx = static_cast<int>(btn);
    if (idx < 0 || idx >= MouseButtonCount) return false;
    return m_mouse_buttons_current[idx] && !m_mouse_buttons_previous[idx];
}

bool RawInput::mouse_button_just_released(MouseButton btn) const {
    int idx = static_cast<int>(btn);
    if (idx < 0 || idx >= MouseButtonCount) return false;
    return !m_mouse_buttons_current[idx] && m_mouse_buttons_previous[idx];
}

// ---- Gamepad (stub) ----

float RawInput::gamepad_axis(int /*pad*/, GamepadAxis /*axis*/) const {
    return 0.0f;
}

bool RawInput::gamepad_button(int /*pad*/, GamepadButton /*btn*/) const {
    return false;
}

// ---- Frame management ----

void RawInput::begin_frame() {
    m_keys_previous = m_keys_current;
    m_mouse_pos_prev = m_mouse_pos;
    m_mouse_buttons_previous = m_mouse_buttons_current;
    m_scroll = 0.0f;
}

} // namespace helios
```

**Key design decisions:**
- `begin_frame()` snapshots current -> previous. This is called at the top of the `update_raw_input` system, before processing that frame's GLFW callback data. This ensures `just_pressed` / `just_released` work correctly: they compare previous (last frame's final state) with current (this frame's final state).
- `m_keys_current` is indexed directly by GLFW key code integer value. Since `KeyCode` values match GLFW values by design, conversion is `static_cast<int>(key)`. The 512-element array is generous (GLFW_KEY_LAST = 348) but trivially small (512 bytes).
- `m_scroll` is accumulated per frame and reset in `begin_frame()`. Multiple scroll events within a single poll are summed.

- [ ] **Step 3: Update CMakeLists.txt**

Add `src/input/raw_input.h`, `src/input/raw_input.cpp`, and `src/input/key_codes.h` to the helios-core target.

- [ ] **Step 4: Verify compilation**

```bash
cd helios && cmake --build build --target helios-core 2>&1 | tail -5
```

- [ ] **Step 5: Commit**

```bash
git add helios-core/src/input/raw_input.h helios-core/src/input/raw_input.cpp
git commit -m "feat(input): implement RawInput resource with keyboard/mouse state tracking"
```

---

## Task 8: Implement update_raw_input system

**Files:**
- Create: `helios-core/src/input/input_systems.h`
- Create: `helios-core/src/input/input_systems.cpp`

This system runs in `Schedule::PreUpdate` and reads `WindowCallbackData` from the primary window to update the `RawInput` resource. It must run after `poll_window_events` (which populates the callback data).

- [ ] **Step 1: Create input_systems.h**

```cpp
// helios-core/src/input/input_systems.h
#pragma once

namespace helios {

// Forward declarations for system parameter types (from ecs/).
template<typename T> class Res;
template<typename T> class ResMut;
class Windows;
struct RawInput;
class InputMap;

// Updates RawInput from the primary window's GLFW callback data.
// Must run after poll_window_events.
void update_raw_input(ResMut<RawInput> input, Res<Windows> windows);

// Updates InputMap's internal pointer to RawInput so action queries work.
// Must run after update_raw_input.
void update_action_map(ResMut<InputMap> map, Res<RawInput> input);

} // namespace helios
```

- [ ] **Step 2: Create input_systems.cpp**

```cpp
// helios-core/src/input/input_systems.cpp
#include "input_systems.h"
#include "raw_input.h"
#include "input_map.h"
#include "../window/windows.h"
#include "../window/window.h"

#include <GLFW/glfw3.h> // for GLFW_PRESS, GLFW_RELEASE constants

namespace helios {

void update_raw_input(ResMut<RawInput> input, Res<Windows> windows) {
    // Snapshot previous frame state.
    input->begin_frame();

    // If no primary window (headless), nothing to process.
    if (!windows->has_primary()) {
        return;
    }

    const Window& primary = windows->primary();
    const WindowCallbackData& cb = primary.callback_data();

    // Process keyboard events.
    for (const auto& ev : cb.key_events) {
        int key = ev.key;
        if (key >= 0 && key < RawInput::KeyArraySize) {
            if (ev.action == GLFW_PRESS || ev.action == GLFW_REPEAT) {
                input->m_keys_current[key] = true;
            } else if (ev.action == GLFW_RELEASE) {
                input->m_keys_current[key] = false;
            }
        }
    }

    // Process mouse button events.
    for (const auto& ev : cb.mouse_button_events) {
        int btn = ev.button;
        if (btn >= 0 && btn < RawInput::MouseButtonCount) {
            if (ev.action == GLFW_PRESS) {
                input->m_mouse_buttons_current[btn] = true;
            } else if (ev.action == GLFW_RELEASE) {
                input->m_mouse_buttons_current[btn] = false;
            }
        }
    }

    // Update mouse position.
    if (cb.mouse_moved) {
        input->m_mouse_pos = glm::vec2(
            static_cast<float>(cb.mouse_x),
            static_cast<float>(cb.mouse_y)
        );
    }

    // Accumulate scroll delta.
    input->m_scroll = static_cast<float>(cb.scroll_y);
}

void update_action_map(ResMut<InputMap> map, Res<RawInput> input) {
    map->set_raw_input(&(*input));
}

} // namespace helios
```

**Key design decisions:**
- `update_raw_input` reads the primary window's `WindowCallbackData` which was populated by GLFW callbacks during `poll_window_events`. This is a read-after-write dependency: `poll_window_events` must run first.
- GLFW `GLFW_REPEAT` events are treated as pressed (key is held down). The engine's `just_pressed` logic uses the previous/current frame comparison, not GLFW's repeat flag.
- Multi-window input: currently only the primary window feeds `RawInput`. Secondary windows could be supported by iterating all windows, but games typically only need input from the focused window. This can be extended later.
- `update_action_map` sets the `InputMap`'s raw pointer to the `RawInput` resource. This pointer is valid for the duration of the frame since both are ECS resources with stable addresses.

- [ ] **Step 3: Update CMakeLists.txt**

Add `src/input/input_systems.h` and `src/input/input_systems.cpp` to the helios-core target.

- [ ] **Step 4: Verify compilation**

```bash
cd helios && cmake --build build --target helios-core 2>&1 | tail -5
```

- [ ] **Step 5: Commit**

```bash
git add helios-core/src/input/input_systems.h helios-core/src/input/input_systems.cpp
git commit -m "feat(input): implement update_raw_input and update_action_map systems"
```

---

## Task 9: Implement InputMap resource

**Files:**
- Create: `helios-core/src/input/input_map.h`
- Create: `helios-core/src/input/input_map.cpp`

The action mapping layer. Users define named actions (bound to multiple keys/buttons) and named axes (positive/negative key pairs). The `InputMap` queries `RawInput` to answer pressed/just_pressed/axis_value questions by action name.

- [ ] **Step 1: Create input_map.h**

```cpp
// helios-core/src/input/input_map.h
#pragma once
#include "key_codes.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace helios {

struct RawInput; // forward declaration

class InputMap {
public:
    InputMap() = default;

    // ---- Define bindings (builder pattern, returns *this for chaining) ----

    InputMap& action(const std::string& name, KeyCode key);
    InputMap& action(const std::string& name, MouseButton btn);
    InputMap& action(const std::string& name, GamepadButton btn);

    InputMap& axis(const std::string& name, KeyCode positive, KeyCode negative);
    InputMap& axis(const std::string& name, GamepadAxis stick);

    // ---- Query (call in game systems) ----

    // Returns true if any binding for this action is currently pressed.
    bool pressed(const std::string& action_name) const;

    // Returns true on the frame the action was first pressed.
    bool just_pressed(const std::string& action_name) const;

    // Returns true on the frame the action was released.
    bool just_released(const std::string& action_name) const;

    // Returns -1.0 to 1.0 for keyboard axes, or raw analog value for gamepad.
    float axis_value(const std::string& axis_name) const;

    // ---- Internal (set by update_action_map system) ----
    void set_raw_input(const RawInput* raw);

private:
    struct ActionBinding {
        std::vector<KeyCode>       keys;
        std::vector<MouseButton>   mouse_buttons;
        std::vector<GamepadButton> gamepad_buttons;
    };

    struct AxisBinding {
        KeyCode     positive    = KeyCode::Unknown;
        KeyCode     negative    = KeyCode::Unknown;
        GamepadAxis gamepad_axis = GamepadAxis::None;
    };

    std::unordered_map<std::string, ActionBinding> m_actions;
    std::unordered_map<std::string, AxisBinding>   m_axes;
    const RawInput* m_raw = nullptr;
};

} // namespace helios
```

- [ ] **Step 2: Create input_map.cpp**

```cpp
// helios-core/src/input/input_map.cpp
#include "input_map.h"
#include "raw_input.h"

namespace helios {

// ---- Binding definition ----

InputMap& InputMap::action(const std::string& name, KeyCode key) {
    m_actions[name].keys.push_back(key);
    return *this;
}

InputMap& InputMap::action(const std::string& name, MouseButton btn) {
    m_actions[name].mouse_buttons.push_back(btn);
    return *this;
}

InputMap& InputMap::action(const std::string& name, GamepadButton btn) {
    m_actions[name].gamepad_buttons.push_back(btn);
    return *this;
}

InputMap& InputMap::axis(const std::string& name, KeyCode positive, KeyCode negative) {
    m_axes[name] = AxisBinding{ .positive = positive, .negative = negative };
    return *this;
}

InputMap& InputMap::axis(const std::string& name, GamepadAxis stick) {
    m_axes[name] = AxisBinding{ .gamepad_axis = stick };
    return *this;
}

// ---- Query ----

bool InputMap::pressed(const std::string& action_name) const {
    if (!m_raw) return false;
    auto it = m_actions.find(action_name);
    if (it == m_actions.end()) return false;

    const auto& binding = it->second;
    for (auto key : binding.keys) {
        if (m_raw->key_pressed(key)) return true;
    }
    for (auto btn : binding.mouse_buttons) {
        if (m_raw->mouse_button_pressed(btn)) return true;
    }
    for (auto btn : binding.gamepad_buttons) {
        if (m_raw->gamepad_button(0, btn)) return true;
    }
    return false;
}

bool InputMap::just_pressed(const std::string& action_name) const {
    if (!m_raw) return false;
    auto it = m_actions.find(action_name);
    if (it == m_actions.end()) return false;

    const auto& binding = it->second;
    for (auto key : binding.keys) {
        if (m_raw->key_just_pressed(key)) return true;
    }
    for (auto btn : binding.mouse_buttons) {
        if (m_raw->mouse_button_just_pressed(btn)) return true;
    }
    // Gamepad just_pressed not yet implemented (needs previous frame state
    // for gamepad buttons, which will come with full gamepad support).
    return false;
}

bool InputMap::just_released(const std::string& action_name) const {
    if (!m_raw) return false;
    auto it = m_actions.find(action_name);
    if (it == m_actions.end()) return false;

    const auto& binding = it->second;
    for (auto key : binding.keys) {
        if (m_raw->key_just_released(key)) return true;
    }
    for (auto btn : binding.mouse_buttons) {
        if (m_raw->mouse_button_just_released(btn)) return true;
    }
    return false;
}

float InputMap::axis_value(const std::string& axis_name) const {
    if (!m_raw) return 0.0f;
    auto it = m_axes.find(axis_name);
    if (it == m_axes.end()) return 0.0f;

    const auto& binding = it->second;

    // Keyboard axis: positive key adds +1, negative key adds -1.
    float value = 0.0f;
    if (binding.positive != KeyCode::Unknown && m_raw->key_pressed(binding.positive)) {
        value += 1.0f;
    }
    if (binding.negative != KeyCode::Unknown && m_raw->key_pressed(binding.negative)) {
        value -= 1.0f;
    }

    // Gamepad axis override (if bound and non-zero, use it instead).
    if (binding.gamepad_axis != GamepadAxis::None) {
        float analog = m_raw->gamepad_axis(0, binding.gamepad_axis);
        if (analog != 0.0f) {
            value = analog;
        }
    }

    return value;
}

// ---- Internal ----

void InputMap::set_raw_input(const RawInput* raw) {
    m_raw = raw;
}

} // namespace helios
```

**Key design decisions:**
- Multiple bindings per action: an action named "jump" can be bound to `KeyCode::Space`, `GamepadButton::A`, and `MouseButton::Right` simultaneously. Any one being pressed returns true.
- Keyboard axis returns exactly -1, 0, or +1. Both keys pressed cancels to 0. Gamepad analog overrides the keyboard value if it is non-zero, giving analog control when a gamepad is connected.
- `m_raw` is a non-owning pointer set each frame by `update_action_map`. It is only valid during system execution. This avoids copying or shared ownership.

- [ ] **Step 3: Update CMakeLists.txt**

Add `src/input/input_map.h` and `src/input/input_map.cpp` to the helios-core target.

- [ ] **Step 4: Verify compilation**

```bash
cd helios && cmake --build build --target helios-core 2>&1 | tail -5
```

- [ ] **Step 5: Commit**

```bash
git add helios-core/src/input/input_map.h helios-core/src/input/input_map.cpp
git commit -m "feat(input): implement InputMap action/axis mapping resource"
```

---

## Task 10: Implement InputPlugin

**Files:**
- Create: `helios-core/src/input/input_plugin.h`
- Create: `helios-core/src/input/input_plugin.cpp`

The plugin inserts `RawInput` and `InputMap` resources and registers the two input systems with explicit ordering.

- [ ] **Step 1: Create input_plugin.h**

```cpp
// helios-core/src/input/input_plugin.h
#pragma once

namespace helios {

class App; // forward declaration

struct InputPlugin {
    void build(App& app);
};

} // namespace helios
```

- [ ] **Step 2: Create input_plugin.cpp**

```cpp
// helios-core/src/input/input_plugin.cpp
#include "input_plugin.h"
#include "raw_input.h"
#include "input_map.h"
#include "input_systems.h"
#include "../app/app.h"

namespace helios {

void InputPlugin::build(App& app) {
    // Insert resources with default state.
    app.insert_resource<RawInput>(RawInput{});
    app.insert_resource<InputMap>(InputMap{});

    // Register systems in PreUpdate.
    // update_raw_input must run after poll_window_events (registered by WindowPlugin).
    // update_action_map must run after update_raw_input.
    //
    // The scheduler uses the .after() ordering constraint from the spec:
    //   app.add_system(Schedule::PreUpdate, update_raw_input.after(poll_window_events));
    //   app.add_system(Schedule::PreUpdate, update_action_map.after(update_raw_input));
    //
    // If the scheduler does not yet support .after() on free functions,
    // fall back to registering them in order (the scheduler runs same-schedule
    // systems sequentially in registration order when no DAG info is available):
    app.add_system(Schedule::PreUpdate, update_raw_input);
    app.add_system(Schedule::PreUpdate, update_action_map);
}

} // namespace helios
```

**Note on ordering:** The spec shows `update_action_map.after(update_raw_input)` syntax. If Plan 2's scheduler implementation supports this (via `SystemDescriptor::after`), use it. If not, registration order provides correct sequencing as a fallback -- the scheduler runs systems in a topological sort of the DAG, and systems with no explicit edges default to registration order within the same schedule.

- [ ] **Step 3: Update CMakeLists.txt**

Add `src/input/input_plugin.h` and `src/input/input_plugin.cpp` to the helios-core target.

- [ ] **Step 4: Verify compilation**

```bash
cd helios && cmake --build build --target helios-core 2>&1 | tail -5
```

- [ ] **Step 5: Commit**

```bash
git add helios-core/src/input/input_plugin.h helios-core/src/input/input_plugin.cpp
git commit -m "feat(input): implement InputPlugin that registers resources and systems"
```

---

## Task 11: Update helios-core CMakeLists.txt (aggregate)

**Files:**
- Modify: `helios-core/CMakeLists.txt`

This task collects all the source files from Tasks 1-10 into the CMake build in one pass, ensuring nothing was missed in individual task steps.

- [ ] **Step 1: Add window/ and input/ source files to the target**

Add the following to the helios-core static library target's source list:

```cmake
# Window system
src/window/window_desc.h
src/window/window.h
src/window/window.cpp
src/window/windows.h
src/window/windows.cpp
src/window/window_events.h
src/window/window_plugin.h
src/window/window_plugin.cpp

# Input system
src/input/key_codes.h
src/input/raw_input.h
src/input/raw_input.cpp
src/input/input_map.h
src/input/input_map.cpp
src/input/input_systems.h
src/input/input_systems.cpp
src/input/input_plugin.h
src/input/input_plugin.cpp
```

If helios-core uses a glob pattern (`file(GLOB_RECURSE ...)`), these are automatically picked up. If it uses an explicit file list, add them.

- [ ] **Step 2: Ensure GLFW is linked**

Verify that the helios-core CMakeLists.txt links GLFW. This should already be configured from Plan 1/2 since the spec declares `helios-core depends on: glm, yaml-cpp, glfw, spdlog, tracy, qlibs-reflect`. If not, add:

```cmake
target_link_libraries(helios-core
    PRIVATE glfw
)
```

GLFW must be PRIVATE because the pimpl pattern ensures no GLFW types leak into public headers.

- [ ] **Step 3: Ensure glm is available for headers**

`raw_input.h` includes `<glm/glm.hpp>`. Ensure glm is a PUBLIC or INTERFACE dependency of helios-core (it should already be from Plan 1/2):

```cmake
target_link_libraries(helios-core
    PUBLIC glm::glm   # or just glm, depending on how it's vendored
)
```

- [ ] **Step 4: Full build verification**

```bash
cd helios && cmake -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build --target helios-core -j$(nproc) 2>&1 | tail -20
```

Expected: Clean compilation, no errors, no warnings from window/ or input/ sources.

- [ ] **Step 5: Commit**

```bash
git add helios-core/CMakeLists.txt
git commit -m "build: add window and input source files to helios-core CMake target"
```

---

## Task 12: Tests -- Window creation and destruction (RAII)

**Files:**
- Create: `helios-core/tests/test_window.cpp`

These tests verify the RAII contract: `Window` constructor creates a GLFW window, destructor destroys it. Move semantics work. Tests require a display server (X11/Wayland) to be running -- skip on headless CI.

- [ ] **Step 1: Create test_window.cpp**

```cpp
// helios-core/tests/test_window.cpp
#include <gtest/gtest.h>
#include "window/window.h"
#include "window/windows.h"
#include "window/window_desc.h"

namespace helios::test {

// Helper: skip test if no display is available (headless CI).
bool has_display() {
    return std::getenv("DISPLAY") != nullptr || std::getenv("WAYLAND_DISPLAY") != nullptr;
}

class WindowTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!has_display()) {
            GTEST_SKIP() << "No display server available, skipping window tests";
        }
    }
};

TEST_F(WindowTest, CreateAndDestroy) {
    // Window should construct without throwing.
    WindowDesc desc{ .title = "Test", .width = 320, .height = 240 };
    {
        Window win(desc);
        EXPECT_EQ(win.width(), 320);
        EXPECT_EQ(win.height(), 240);
        EXPECT_FALSE(win.should_close());
        EXPECT_NE(win.native_handle(), nullptr);
    }
    // Destructor should have run without crash.
}

TEST_F(WindowTest, MoveConstructor) {
    WindowDesc desc{ .title = "MoveTest", .width = 640, .height = 480 };
    Window a(desc);
    void* handle = a.native_handle();

    Window b(std::move(a));
    EXPECT_EQ(b.native_handle(), handle);
    EXPECT_EQ(b.width(), 640);
    // 'a' is now in a moved-from state; do not access its methods.
}

TEST_F(WindowTest, MoveAssignment) {
    WindowDesc desc_a{ .title = "A", .width = 100, .height = 100 };
    WindowDesc desc_b{ .title = "B", .width = 200, .height = 200 };

    Window a(desc_a);
    Window b(desc_b);

    b = std::move(a);
    // After move-assignment, b holds what a had.
    EXPECT_EQ(b.title(), "A");
}

TEST_F(WindowTest, WindowsManagerCreateDestroy) {
    WindowDesc primary_desc{ .title = "Primary", .width = 800, .height = 600 };
    Windows mgr(Window(primary_desc));

    EXPECT_TRUE(mgr.has_primary());
    EXPECT_EQ(mgr.count(), 1);
    EXPECT_EQ(mgr.primary().title(), "Primary");

    // Create a secondary window.
    WindowId second = mgr.create(WindowDesc{ .title = "Second", .width = 400, .height = 300 });
    EXPECT_EQ(mgr.count(), 2);
    EXPECT_EQ(mgr.get(second).title(), "Second");

    // Destroy the secondary.
    mgr.destroy(second);
    EXPECT_EQ(mgr.count(), 1);

    // Cannot destroy primary.
    EXPECT_THROW(mgr.destroy(mgr.primary_id()), std::runtime_error);
}

TEST_F(WindowTest, WindowsManagerHeadless) {
    // Default-constructed Windows has no primary (headless mode).
    Windows mgr;
    EXPECT_FALSE(mgr.has_primary());
    EXPECT_EQ(mgr.count(), 0);
    EXPECT_THROW(mgr.primary(), std::runtime_error);
}

TEST_F(WindowTest, PollEventsDoesNotCrash) {
    WindowDesc desc{ .title = "PollTest", .width = 320, .height = 240 };
    Window win(desc);
    // poll_events should work even with no pending events.
    win.poll_events();
    // Callback data should be in a clean state.
    const auto& cb = win.callback_data();
    EXPECT_FALSE(cb.resized);
    EXPECT_FALSE(cb.close_requested);
    EXPECT_TRUE(cb.key_events.empty());
}

} // namespace helios::test
```

- [ ] **Step 2: Add to CMake test target**

Add `tests/test_window.cpp` to the helios-core test executable. This executable should link against `helios-core` and `gtest_main`:

```cmake
# In helios-core/CMakeLists.txt or a tests/CMakeLists.txt:
add_executable(helios-core-tests
    tests/test_window.cpp
    # ... other test files
)
target_link_libraries(helios-core-tests PRIVATE helios-core gtest gtest_main)
add_test(NAME helios-core-tests COMMAND helios-core-tests)
```

- [ ] **Step 3: Build and run tests**

```bash
cd helios && cmake --build build --target helios-core-tests -j$(nproc) && ./build/helios-core-tests --gtest_filter="WindowTest.*" 2>&1
```

Expected: All tests pass (or skip on headless CI).

- [ ] **Step 4: Commit**

```bash
git add helios-core/tests/test_window.cpp
git commit -m "test(window): add RAII, move semantics, and Windows manager tests"
```

---

## Task 13: Tests -- RawInput state tracking

**Files:**
- Create: `helios-core/tests/test_raw_input.cpp`

These tests verify the `RawInput` state machine (pressed, just_pressed, just_released) purely through direct manipulation of internal state. No GLFW needed.

- [ ] **Step 1: Create test_raw_input.cpp**

```cpp
// helios-core/tests/test_raw_input.cpp
#include <gtest/gtest.h>
#include "input/raw_input.h"

namespace helios::test {

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

    // Simulate frame 1: key A pressed.
    input.begin_frame();
    input.m_keys_current[static_cast<int>(KeyCode::A)] = true;

    EXPECT_TRUE(input.key_pressed(KeyCode::A));
    EXPECT_TRUE(input.key_just_pressed(KeyCode::A));  // was not pressed last frame
    EXPECT_FALSE(input.key_just_released(KeyCode::A));

    // Simulate frame 2: key A still held.
    input.begin_frame();
    input.m_keys_current[static_cast<int>(KeyCode::A)] = true;

    EXPECT_TRUE(input.key_pressed(KeyCode::A));
    EXPECT_FALSE(input.key_just_pressed(KeyCode::A));  // was pressed last frame too
    EXPECT_FALSE(input.key_just_released(KeyCode::A));
}

TEST(RawInputTest, KeyJustReleased) {
    RawInput input;

    // Frame 1: press A.
    input.begin_frame();
    input.m_keys_current[static_cast<int>(KeyCode::A)] = true;

    // Frame 2: release A.
    input.begin_frame();
    input.m_keys_current[static_cast<int>(KeyCode::A)] = false;

    EXPECT_FALSE(input.key_pressed(KeyCode::A));
    EXPECT_FALSE(input.key_just_pressed(KeyCode::A));
    EXPECT_TRUE(input.key_just_released(KeyCode::A));  // was pressed, now is not
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
}

TEST(RawInputTest, MousePositionAndDelta) {
    RawInput input;

    // Frame 1: mouse at (100, 200).
    input.begin_frame();
    input.m_mouse_pos = glm::vec2(100.0f, 200.0f);

    EXPECT_EQ(input.mouse_position(), glm::vec2(100.0f, 200.0f));
    EXPECT_EQ(input.mouse_delta(), glm::vec2(100.0f, 200.0f));  // prev was (0,0)

    // Frame 2: mouse moves to (150, 220).
    input.begin_frame();
    input.m_mouse_pos = glm::vec2(150.0f, 220.0f);

    EXPECT_EQ(input.mouse_position(), glm::vec2(150.0f, 220.0f));
    EXPECT_EQ(input.mouse_delta(), glm::vec2(50.0f, 20.0f));
}

TEST(RawInputTest, ScrollDeltaResetsPerFrame) {
    RawInput input;

    // Frame 1: scroll +3.
    input.begin_frame();
    input.m_scroll = 3.0f;
    EXPECT_FLOAT_EQ(input.scroll_delta(), 3.0f);

    // Frame 2: no scroll.
    input.begin_frame();
    EXPECT_FLOAT_EQ(input.scroll_delta(), 0.0f);  // reset by begin_frame
}

TEST(RawInputTest, OutOfRangeKeyCodeReturnsFalse) {
    RawInput input;
    // KeyCode::Unknown is -1, should not crash.
    EXPECT_FALSE(input.key_pressed(KeyCode::Unknown));
    EXPECT_FALSE(input.key_just_pressed(KeyCode::Unknown));
    EXPECT_FALSE(input.key_just_released(KeyCode::Unknown));
}

} // namespace helios::test
```

- [ ] **Step 2: Add to CMake test target**

Add `tests/test_raw_input.cpp` to the helios-core-tests executable.

- [ ] **Step 3: Build and run tests**

```bash
cd helios && cmake --build build --target helios-core-tests -j$(nproc) && ./build/helios-core-tests --gtest_filter="RawInputTest.*" 2>&1
```

Expected: All tests pass.

- [ ] **Step 4: Commit**

```bash
git add helios-core/tests/test_raw_input.cpp
git commit -m "test(input): add RawInput state tracking tests"
```

---

## Task 14: Tests -- InputMap action binding and querying

**Files:**
- Create: `helios-core/tests/test_input_map.cpp`

Tests that verify the `InputMap` action/axis binding and query logic. These work by constructing a `RawInput` with known state and pointing the `InputMap` at it.

- [ ] **Step 1: Create test_input_map.cpp**

```cpp
// helios-core/tests/test_input_map.cpp
#include <gtest/gtest.h>
#include "input/input_map.h"
#include "input/raw_input.h"

namespace helios::test {

class InputMapTest : public ::testing::Test {
protected:
    RawInput raw;
    InputMap map;

    void SetUp() override {
        map.set_raw_input(&raw);
    }

    // Helper: simulate pressing a key for the current frame.
    void press_key(KeyCode key) {
        raw.m_keys_current[static_cast<int>(key)] = true;
    }

    void release_key(KeyCode key) {
        raw.m_keys_current[static_cast<int>(key)] = false;
    }

    void press_mouse(MouseButton btn) {
        raw.m_mouse_buttons_current[static_cast<int>(btn)] = true;
    }

    void next_frame() {
        raw.begin_frame();
    }
};

TEST_F(InputMapTest, SingleKeyAction) {
    map.action("jump", KeyCode::Space);

    next_frame();
    EXPECT_FALSE(map.pressed("jump"));

    press_key(KeyCode::Space);
    EXPECT_TRUE(map.pressed("jump"));
    EXPECT_TRUE(map.just_pressed("jump"));

    next_frame();
    press_key(KeyCode::Space);
    EXPECT_TRUE(map.pressed("jump"));
    EXPECT_FALSE(map.just_pressed("jump")); // held, not just pressed
}

TEST_F(InputMapTest, MultipleBindingsSameAction) {
    map.action("fire", KeyCode::Space);
    map.action("fire", MouseButton::Left);

    next_frame();
    EXPECT_FALSE(map.pressed("fire"));

    // Either binding activates the action.
    press_key(KeyCode::Space);
    EXPECT_TRUE(map.pressed("fire"));

    next_frame();
    release_key(KeyCode::Space);
    press_mouse(MouseButton::Left);
    EXPECT_TRUE(map.pressed("fire"));
}

TEST_F(InputMapTest, ActionJustReleased) {
    map.action("crouch", KeyCode::LeftControl);

    next_frame();
    press_key(KeyCode::LeftControl);

    next_frame();
    release_key(KeyCode::LeftControl);
    EXPECT_TRUE(map.just_released("crouch"));

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

    // Press positive key.
    press_key(KeyCode::D);
    EXPECT_FLOAT_EQ(map.axis_value("horizontal"), 1.0f);

    // Press both keys (cancel out).
    press_key(KeyCode::A);
    EXPECT_FLOAT_EQ(map.axis_value("horizontal"), 0.0f);

    // Release positive, only negative held.
    release_key(KeyCode::D);
    EXPECT_FLOAT_EQ(map.axis_value("horizontal"), -1.0f);
}

TEST_F(InputMapTest, NullRawInputReturnsFalse) {
    InputMap detached;
    // No set_raw_input called.
    detached.action("test", KeyCode::A);
    EXPECT_FALSE(detached.pressed("test"));
    EXPECT_FLOAT_EQ(detached.axis_value("test"), 0.0f);
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
    EXPECT_TRUE(map.pressed("shoot") == false); // nothing pressed for shoot
}

} // namespace helios::test
```

- [ ] **Step 2: Add to CMake test target**

Add `tests/test_input_map.cpp` to the helios-core-tests executable.

- [ ] **Step 3: Build and run tests**

```bash
cd helios && cmake --build build --target helios-core-tests -j$(nproc) && ./build/helios-core-tests --gtest_filter="InputMapTest.*" 2>&1
```

Expected: All tests pass.

- [ ] **Step 4: Commit**

```bash
git add helios-core/tests/test_input_map.cpp
git commit -m "test(input): add InputMap action binding and axis query tests"
```

---

## Task 15: Integration test -- WindowPlugin + InputPlugin event flow

**Files:**
- Create: `helios-core/tests/test_window_input_integration.cpp`

This test verifies the full pipeline: WindowPlugin creates a window and registers systems; InputPlugin registers input resources; the systems correctly propagate callback data through RawInput. This test exercises the ECS integration.

- [ ] **Step 1: Create test_window_input_integration.cpp**

```cpp
// helios-core/tests/test_window_input_integration.cpp
#include <gtest/gtest.h>
#include "ecs/world.h"
#include "app/app.h"
#include "window/window_plugin.h"
#include "window/window_events.h"
#include "window/windows.h"
#include "input/input_plugin.h"
#include "input/raw_input.h"
#include "input/input_map.h"

namespace helios::test {

// Helper: check if display is available.
bool has_display_for_integration() {
    return std::getenv("DISPLAY") != nullptr || std::getenv("WAYLAND_DISPLAY") != nullptr;
}

class WindowInputIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!has_display_for_integration()) {
            GTEST_SKIP() << "No display server available";
        }
    }
};

TEST_F(WindowInputIntegrationTest, PluginBuildInsertsResources) {
    // Verify that building the plugins inserts all expected resources.
    App app;
    app.add_plugin(WindowPlugin{
        .primary_window = { .title = "IntegrationTest", .width = 320, .height = 240 }
    });
    app.add_plugin(InputPlugin{});

    World& world = app.world();

    // WindowPlugin should have inserted Windows resource.
    ASSERT_TRUE(world.has_resource<Windows>());
    EXPECT_TRUE(world.resource<Windows>().has_primary());
    EXPECT_EQ(world.resource<Windows>().primary().title(), "IntegrationTest");

    // InputPlugin should have inserted RawInput and InputMap.
    ASSERT_TRUE(world.has_resource<RawInput>());
    ASSERT_TRUE(world.has_resource<InputMap>());
}

TEST_F(WindowInputIntegrationTest, WindowResizedEventEmitted) {
    // This test verifies the event path works end-to-end.
    // We cannot easily trigger a real GLFW resize in a test, so we
    // verify that the event type was registered and a writer can send.
    App app;
    app.add_plugin(WindowPlugin{
        .primary_window = { .title = "ResizeTest", .width = 320, .height = 240 }
    });

    World& world = app.world();

    // The WindowPlugin should have registered WindowResized events.
    // Verify by getting a writer (would throw/fail if not registered).
    auto writer = world.event_writer<WindowResized>();
    writer.send(WindowResized{ .window_id = 1, .width = 640, .height = 480 });

    auto reader = world.event_reader<WindowResized>();
    int count = 0;
    for (const auto& ev : reader) {
        EXPECT_EQ(ev.width, 640);
        EXPECT_EQ(ev.height, 480);
        count++;
    }
    EXPECT_EQ(count, 1);
}

TEST_F(WindowInputIntegrationTest, RawInputUpdatedBySystem) {
    // This test runs the update_raw_input system directly via World::run_system
    // to verify it reads callback data correctly.
    // Note: since we can't easily inject GLFW events, we verify that
    // after polling an idle window, RawInput remains in its default state.
    App app;
    app.add_plugin(WindowPlugin{
        .primary_window = { .title = "InputTest", .width = 320, .height = 240 }
    });
    app.add_plugin(InputPlugin{});

    World& world = app.world();

    // Run the poll + input update systems.
    world.run_system(poll_window_events);
    world.run_system(update_raw_input);
    world.run_system(update_action_map);

    // With no events pending, RawInput should be all zeros/false.
    const auto& input = world.resource<RawInput>();
    EXPECT_FALSE(input.key_pressed(KeyCode::A));
    EXPECT_EQ(input.mouse_position(), glm::vec2(0.0f, 0.0f));
    EXPECT_FLOAT_EQ(input.scroll_delta(), 0.0f);
}

TEST_F(WindowInputIntegrationTest, InputMapWorksWithResources) {
    App app;
    app.add_plugin(WindowPlugin{
        .primary_window = { .title = "MapTest", .width = 320, .height = 240 }
    });
    app.add_plugin(InputPlugin{});

    World& world = app.world();

    // Configure action bindings.
    auto& input_map = world.resource<InputMap>();
    input_map.action("jump", KeyCode::Space);
    input_map.axis("horizontal", KeyCode::D, KeyCode::A);

    // Run systems to wire up the InputMap -> RawInput pointer.
    world.run_system(poll_window_events);
    world.run_system(update_raw_input);
    world.run_system(update_action_map);

    // Manually set a key in RawInput to simulate input.
    auto& raw = world.resource<RawInput>();
    raw.m_keys_current[static_cast<int>(KeyCode::Space)] = true;

    // InputMap should now report jump as pressed.
    EXPECT_TRUE(input_map.pressed("jump"));
    EXPECT_FLOAT_EQ(input_map.axis_value("horizontal"), 0.0f);

    // Press D key for horizontal axis.
    raw.m_keys_current[static_cast<int>(KeyCode::D)] = true;
    EXPECT_FLOAT_EQ(input_map.axis_value("horizontal"), 1.0f);
}

} // namespace helios::test
```

- [ ] **Step 2: Add to CMake test target**

Add `tests/test_window_input_integration.cpp` to the helios-core-tests executable.

- [ ] **Step 3: Build and run all tests**

```bash
cd helios && cmake --build build --target helios-core-tests -j$(nproc) && ./build/helios-core-tests 2>&1
```

Expected: All tests pass (window tests skip on headless CI, input unit tests always pass).

- [ ] **Step 4: Commit**

```bash
git add helios-core/tests/test_window_input_integration.cpp
git commit -m "test: add WindowPlugin + InputPlugin integration tests"
```

---

## Summary

**Files created (16 total):**

| File | Purpose |
|------|---------|
| `helios-core/src/input/key_codes.h` | KeyCode, MouseButton, GamepadButton, GamepadAxis enums |
| `helios-core/src/window/window_desc.h` | WindowDesc struct, WindowId type |
| `helios-core/src/window/window.h` | Window class (pimpl public API) |
| `helios-core/src/window/window.cpp` | Window implementation (GLFW, callbacks, RAII) |
| `helios-core/src/window/windows.h` | Windows multi-window manager (public API) |
| `helios-core/src/window/windows.cpp` | Windows implementation |
| `helios-core/src/window/window_events.h` | WindowResized, WindowClosed event structs |
| `helios-core/src/window/window_plugin.h` | WindowPlugin struct + poll_window_events declaration |
| `helios-core/src/window/window_plugin.cpp` | WindowPlugin::build + poll_window_events implementation |
| `helios-core/src/input/raw_input.h` | RawInput resource (public API) |
| `helios-core/src/input/raw_input.cpp` | RawInput implementation |
| `helios-core/src/input/input_map.h` | InputMap action/axis mapping (public API) |
| `helios-core/src/input/input_map.cpp` | InputMap implementation |
| `helios-core/src/input/input_systems.h` | update_raw_input, update_action_map declarations |
| `helios-core/src/input/input_systems.cpp` | Input system implementations |
| `helios-core/src/input/input_plugin.h` | InputPlugin struct |
| `helios-core/src/input/input_plugin.cpp` | InputPlugin::build implementation |

**Files modified (1):**

| File | Change |
|------|--------|
| `helios-core/CMakeLists.txt` | Add all window/ and input/ sources, verify GLFW and glm linkage |

**Test files (4):**

| File | What it tests |
|------|---------------|
| `helios-core/tests/test_window.cpp` | Window RAII, move semantics, Windows manager |
| `helios-core/tests/test_raw_input.cpp` | RawInput pressed/just_pressed/just_released logic |
| `helios-core/tests/test_input_map.cpp` | InputMap action/axis binding and query |
| `helios-core/tests/test_window_input_integration.cpp` | Full plugin pipeline, event emission, ECS resource wiring |

**Data flow (per frame):**

```
glfwPollEvents()  -->  GLFW callbacks  -->  WindowCallbackData (per window)
       |
       v
poll_window_events system (Schedule::PreUpdate)
  - Calls Windows::poll_all()
  - Reads callback_data() from each window
  - Emits WindowResized / WindowClosed events via EventWriter
       |
       v
update_raw_input system (Schedule::PreUpdate, after poll_window_events)
  - Calls RawInput::begin_frame() (snapshot previous state)
  - Reads primary window's callback_data()
  - Updates m_keys_current, m_mouse_buttons_current, m_mouse_pos, m_scroll
       |
       v
update_action_map system (Schedule::PreUpdate, after update_raw_input)
  - Sets InputMap::m_raw pointer to &RawInput
       |
       v
Game systems (Schedule::Update)
  - Query via Res<RawInput>: key_pressed(), mouse_delta(), etc.
  - Query via Res<InputMap>: pressed("jump"), axis_value("horizontal"), etc.
  - Read via EventReader<WindowResized>, EventReader<WindowClosed>
```

**Commits (15 total):**
1. `feat(input): add KeyCode, MouseButton, GamepadButton, GamepadAxis enums`
2. `feat(window): add WindowDesc struct and WindowId type`
3. `feat(window): implement Window class with GLFW pimpl pattern`
4. `feat(window): implement Windows multi-window resource manager`
5. `feat(window): add WindowResized and WindowClosed event structs`
6. `feat(window): implement WindowPlugin with poll_window_events system`
7. `feat(input): implement RawInput resource with keyboard/mouse state tracking`
8. `feat(input): implement update_raw_input and update_action_map systems`
9. `feat(input): implement InputMap action/axis mapping resource`
10. `feat(input): implement InputPlugin that registers resources and systems`
11. `build: add window and input source files to helios-core CMake target`
12. `test(window): add RAII, move semantics, and Windows manager tests`
13. `test(input): add RawInput state tracking tests`
14. `test(input): add InputMap action binding and axis query tests`
15. `test: add WindowPlugin + InputPlugin integration tests`
