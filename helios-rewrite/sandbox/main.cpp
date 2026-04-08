// Helios Engine - User Sandbox
// Demonstrates: ECS entity spawning AND multi-window rendering via abstract RHI.
// Two windows are displayed: primary (dark blue) and secondary (dark red).
// Uses ONLY abstract RHI interfaces -- no Vulkan-specific headers.

#include <helios/ecs/ecs.h>
#include <helios/components/components.h>
#include <helios/core/logging.h>
#include <helios/window/window.h>
#include <helios/window/window_types.h>

// Abstract RHI interfaces only -- NO vulkan/ headers
#include <helios/rhi/rhi.h>
#include <helios/rhi/rhi_factory.h>

#include <GLFW/glfw3.h>
#include <cmath>
#include <memory>

using namespace helios;
using namespace helios::rhi;

// ============================================================
// Log channels
// ============================================================

HELIOS_DEFINE_LOG_CHANNEL(Game);
HELIOS_DEFINE_LOG_CHANNEL(Scene);

// ============================================================
// Game components (ECS demo)
// ============================================================

struct Velocity { glm::vec3 value{0.0f}; };
struct Health   { float current = 100.0f; float max = 100.0f; };
struct Enemy    { float speed = 5.0f; };
struct Player   {};

// ============================================================
// Render one frame for a single window
// ============================================================

static bool render_window(Device& device, Swapchain& swapchain,
                          CommandBuffer& cmd, const ClearValues& clear)
{
    // 1. Acquire next swapchain image
    if (!swapchain.acquire_next_image())
        return false;  // swapchain out of date -- skip this frame

    // 2. Record command buffer
    cmd.begin();
    swapchain.begin_rendering(cmd, clear);
    // (draw calls would go here)
    swapchain.end_rendering(cmd);
    cmd.end();

    // 3. Submit with swapchain sync objects
    device.submit_for_present(cmd, swapchain);

    // 4. Present
    swapchain.present();

    return true;
}

// ============================================================
// Main
// ============================================================

int main()
{
    // --- Logging ---
    LogSystem log(LogConfig{
        .enable_file_sink = false,
        .default_level    = LogLevel::Debug,
    });

    HELIOS_LOG(Core, Info, "=== Helios Engine Sandbox ===");

    // --- GLFW init (needed before creating windows) ---
    if (!glfwInit()) {
        HELIOS_LOG(Core, Error, "Failed to initialize GLFW");
        return 1;
    }

    // --- Enumerate GPUs ---
    auto gpus = enumerate_devices(Backend::Vulkan);
    HELIOS_LOG(Core, Info, "Available GPUs:");
    for (auto& gpu : gpus) {
        const char* type_str = "Other";
        switch (gpu.type) {
            case GpuType::Discrete:   type_str = "Discrete"; break;
            case GpuType::Integrated: type_str = "Integrated"; break;
            case GpuType::Virtual:    type_str = "Virtual"; break;
            default: break;
        }
        HELIOS_LOG(Core, Info, "  [{}] {} ({}) - VRAM: {} MB, API: {}",
            gpu.index, gpu.name, type_str,
            gpu.vram_bytes / (1024 * 1024), gpu.api_version);
    }

    // --- Create two windows (unique_ptr for explicit destruction order control) ---
    auto primary_win = std::make_unique<Window>(WindowDesc{
        .title  = "Helios Sandbox - Primary (Blue)",
        .width  = 1280,
        .height = 720,
    });
    auto secondary_win = std::make_unique<Window>(WindowDesc{
        .title  = "Helios Sandbox - Secondary (Red)",
        .width  = 640,
        .height = 480,
    });

    HELIOS_LOG(Core, Info, "Primary window:   {}x{}", primary_win->width(), primary_win->height());
    HELIOS_LOG(Core, Info, "Secondary window: {}x{}", secondary_win->width(), secondary_win->height());

    // --- ECS demo: spawn some entities to show the ECS still works ---
    {
        World world;
        world.spawn(
            Transform{ .position = glm::vec3{0, 5, -10} },
            Camera{}, ActiveCamera{}, Tag{ .name = "camera" });
        world.spawn(
            Transform{ .position = glm::vec3{0, 0, 0} },
            Player{}, Tag{ .name = "player" });
        for (int i = 0; i < 5; i++) {
            float angle = static_cast<float>(i) / 5.0f * 6.28318f;
            world.spawn(
                Transform{ .position = glm::vec3{
                    std::cos(angle) * 8.0f, 0, std::sin(angle) * 8.0f} },
                Enemy{ .speed = 1.0f + static_cast<float>(i) },
                Tag{ .name = std::string("enemy_") + std::to_string(i) });
        }
        HELIOS_LOG(Scene, Info, "ECS demo: spawned camera + player + 5 enemies");
    }

    // --- Create device via RHI factory (uses primary window for initial surface) ---
    auto device = create_device(Backend::Vulkan, "HeliosSandbox",
        static_cast<GLFWwindow*>(primary_win->native_handle()));
    if (!device) {
        HELIOS_LOG(Core, Error, "Failed to create RHI device");
        return 1;
    }

    // --- Create primary swapchain ---
    auto primary_swapchain = device->create_swapchain(SwapchainDesc{
        .width   = primary_win->width(),
        .height  = primary_win->height(),
    });

    // --- Create secondary surface + swapchain ---
    void* secondary_surface = device->create_surface(secondary_win.native_handle());
    if (!secondary_surface) {
        HELIOS_LOG(Core, Error, "Failed to create secondary surface");
        return 1;
    }

    auto secondary_swapchain = device->create_swapchain(SwapchainDesc{
        .width   = secondary_win->width(),
        .height  = secondary_win->height(),
        .surface = secondary_surface,
    });

    // --- Create command buffers ---
    auto primary_cmd   = device->create_command_buffer();
    auto secondary_cmd = device->create_command_buffer();

    HELIOS_LOG(Core, Info, "RHI initialized: device + 2 swapchains + 2 command buffers");
    HELIOS_LOG(Core, Info, "Starting render loop");
    HELIOS_LOG(Core, Info, "  Primary  = dark blue (0.1, 0.1, 0.3)");
    HELIOS_LOG(Core, Info, "  Secondary = dark red  (0.3, 0.1, 0.1)");
    HELIOS_LOG(Core, Info, "  Close primary to quit. Secondary can close independently.");

    // Clear colors
    ClearValues blue_clear;
    blue_clear.color[0] = 0.1f;
    blue_clear.color[1] = 0.1f;
    blue_clear.color[2] = 0.3f;
    blue_clear.color[3] = 1.0f;

    ClearValues red_clear;
    red_clear.color[0] = 0.3f;
    red_clear.color[1] = 0.1f;
    red_clear.color[2] = 0.1f;
    red_clear.color[3] = 1.0f;

    bool secondary_alive = true;

    // --- Main loop: runs until primary window closes ---
    while (!primary_win->should_close()) {
        glfwPollEvents();

        // Always render to primary
        render_window(*device, *primary_swapchain, *primary_cmd, blue_clear);

        // Handle secondary window lifecycle
        if (secondary_alive) {
            if (secondary_win->should_close()) {
                HELIOS_LOG(Core, Info, "Secondary window closed -- cleaning up");
                device->wait_idle();
                secondary_cmd.reset();
                secondary_swapchain.reset();
                device->destroy_surface(secondary_surface);
                secondary_surface = nullptr;
                secondary_win.reset();  // destroy GLFW window after its Vulkan resources
                secondary_alive = false;
            } else {
                render_window(*device, *secondary_swapchain, *secondary_cmd, red_clear);
            }
        }
    }

    // --- Cleanup: destroy resources in correct order (children before device) ---
    HELIOS_LOG(Core, Info, "Primary window closed -- shutting down");
    device->wait_idle();

    // Destroy secondary resources if still alive
    if (secondary_alive) {
        secondary_cmd.reset();
        secondary_swapchain.reset();
        device->destroy_surface(secondary_surface);
        secondary_surface = nullptr;
    }

    // Destroy primary GPU resources
    primary_cmd.reset();
    primary_swapchain.reset();

    // Destroy GLFW windows BEFORE VkInstance (GLFW/Wayland cleanup needs Vulkan alive)
    secondary_win.reset();
    primary_win.reset();

    // Device last (destroys VkDevice, primary surface, VkInstance)
    device.reset();

    HELIOS_LOG(Core, Info, "=== Sandbox shutdown ===");
    return 0;
}
