// helios-core/src/helios/window/window.cpp
#include "helios/window/window.h"
#include "helios/core/assert.h"
#include "helios/core/log_macros.h"

#include <GLFW/glfw3.h>
#include <atomic>

// Define the Window log channel
HELIOS_DEFINE_LOG_CHANNEL(Window);

namespace helios {

// ---- GLFW lifetime management (reference counted) ----
static std::atomic<int> s_glfw_ref_count{0};

static void ensure_glfw_init() {
    if (s_glfw_ref_count.fetch_add(1, std::memory_order_relaxed) == 0) {
        glfwSetErrorCallback([](int code, const char* msg) {
            HELIOS_LOG(Window, Error, "GLFW error {}: {}", code, msg);
        });
        if (!glfwInit()) {
            s_glfw_ref_count.fetch_sub(1, std::memory_order_relaxed);
            HELIOS_LOG(Window, Fatal, "Failed to initialize GLFW");
        }
        HELIOS_LOG(Window, Info, "GLFW initialized");
    }
}

static void release_glfw() {
    if (s_glfw_ref_count.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        glfwTerminate();
        HELIOS_LOG(Window, Info, "GLFW terminated");
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
            HELIOS_ASSERT(mode, "Failed to get video mode for primary monitor");
            w = mode->width;
            h = mode->height;
        }

        glfw_window = glfwCreateWindow(w, h, d.title.c_str(), monitor, nullptr);
        if (!glfw_window) {
            release_glfw();
            HELIOS_LOG(Window, Fatal, "Failed to create GLFW window \"{}\" ({}x{})",
                       d.title, d.width, d.height);
        }

        HELIOS_LOG(Window, Info, "Created window \"{}\" ({}x{})", d.title, w, h);

        // Store pointer to callback data so GLFW callbacks can reach it.
        glfwSetWindowUserPointer(glfw_window, &cb_data);

        // -- Install callbacks --
        glfwSetKeyCallback(glfw_window,
            [](GLFWwindow* win, int key, int /*scancode*/, int action, int /*mods*/) {
                auto* data = static_cast<WindowCallbackData*>(glfwGetWindowUserPointer(win));
                data->key_events.push_back({key, action});
            });

        glfwSetMouseButtonCallback(glfw_window,
            [](GLFWwindow* win, int button, int action, int /*mods*/) {
                auto* data = static_cast<WindowCallbackData*>(glfwGetWindowUserPointer(win));
                data->mouse_button_events.push_back({button, action});
            });

        glfwSetCursorPosCallback(glfw_window,
            [](GLFWwindow* win, double x, double y) {
                auto* data = static_cast<WindowCallbackData*>(glfwGetWindowUserPointer(win));
                data->mouse_x = x;
                data->mouse_y = y;
                data->mouse_moved = true;
            });

        glfwSetScrollCallback(glfw_window,
            [](GLFWwindow* win, double xoff, double yoff) {
                auto* data = static_cast<WindowCallbackData*>(glfwGetWindowUserPointer(win));
                data->scroll_x += xoff;
                data->scroll_y += yoff;
            });

        glfwSetWindowSizeCallback(glfw_window,
            [](GLFWwindow* win, int width, int height) {
                auto* data = static_cast<WindowCallbackData*>(glfwGetWindowUserPointer(win));
                data->resized = true;
                data->new_width  = static_cast<uint32_t>(width);
                data->new_height = static_cast<uint32_t>(height);
            });

        glfwSetWindowCloseCallback(glfw_window,
            [](GLFWwindow* win) {
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
            HELIOS_LOG(Window, Debug, "Destroying window \"{}\"", desc.title);
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

const WindowCallbackData& Window::callback_data() const {
    return m_impl->cb_data;
}

void* Window::native_handle() const {
    return static_cast<void*>(m_impl->glfw_window);
}

void Window::set_cursor_mode(CursorMode mode) {
    if (mode == CursorMode::Captured) {
        glfwSetInputMode(m_impl->glfw_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        if (glfwRawMouseMotionSupported())
            glfwSetInputMode(m_impl->glfw_window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
    } else {
        glfwSetInputMode(m_impl->glfw_window, GLFW_RAW_MOUSE_MOTION, GLFW_FALSE);
        glfwSetInputMode(m_impl->glfw_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
}

Window::CursorMode Window::cursor_mode() const {
    int mode = glfwGetInputMode(m_impl->glfw_window, GLFW_CURSOR);
    return (mode == GLFW_CURSOR_DISABLED) ? CursorMode::Captured : CursorMode::Normal;
}

} // namespace helios
