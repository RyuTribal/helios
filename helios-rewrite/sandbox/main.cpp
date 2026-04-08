// Helios Engine - User Sandbox
// Demonstrates: ECS entity spawning AND multi-window Vulkan rendering.
// Two windows are displayed: primary (dark blue) and secondary (dark red).
// The main loop is manual (no app.run()) so we can drive Vulkan directly.

#include <helios/ecs/ecs.h>
#include <helios/components/components.h>
#include <helios/core/logging.h>
#include <helios/window/window.h>
#include <helios/window/windows.h>
#include <helios/window/window_types.h>

// Vulkan rendering
#include <helios/vulkan/vulkan_context.h>
#include <helios/vulkan/vulkan_device.h>
#include <helios/vulkan/vulkan_swapchain.h>
#include <helios/vulkan/vulkan_command_buffer.h>
#include <helios/rhi/rhi_types.h>

#include <GLFW/glfw3.h>
#include <cmath>
#include <memory>
#include <vector>

using namespace helios;
using namespace helios::rhi;
using namespace helios::rhi::vulkan;

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
// Per-window Vulkan state
// ============================================================

struct WindowRenderState {
    VkSurfaceKHR           surface = VK_NULL_HANDLE;
    VulkanSwapchain        swapchain;
    VulkanCommandBuffer    cmd;
    float                  clear_color[4] = {0, 0, 0, 1};
    const char*            label = "";
};

// ============================================================
// Helpers: image layout transitions via VkImageMemoryBarrier2
// ============================================================

static void transition_image(VkCommandBuffer cmd, VkImage image,
                             VkImageLayout old_layout, VkImageLayout new_layout,
                             VkPipelineStageFlags2 src_stage, VkAccessFlags2 src_access,
                             VkPipelineStageFlags2 dst_stage, VkAccessFlags2 dst_access)
{
    VkImageMemoryBarrier2 barrier{};
    barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    barrier.srcStageMask        = src_stage;
    barrier.srcAccessMask       = src_access;
    barrier.dstStageMask        = dst_stage;
    barrier.dstAccessMask       = dst_access;
    barrier.oldLayout           = old_layout;
    barrier.newLayout           = new_layout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image               = image;
    barrier.subresourceRange    = {
        VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1
    };

    VkDependencyInfo dep{};
    dep.sType                    = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dep.imageMemoryBarrierCount  = 1;
    dep.pImageMemoryBarriers     = &barrier;

    vkCmdPipelineBarrier2(cmd, &dep);
}

// ============================================================
// Render one frame for a single window
// ============================================================

static bool render_window(VulkanDevice& device, WindowRenderState& ws)
{
    // 1. Acquire
    if (!ws.swapchain.acquire_next_image())
        return false;  // swapchain out of date -- skip this frame

    VkCommandBuffer raw_cmd = ws.cmd.vk_command_buffer();
    VkImage image = ws.swapchain.current_vk_image();

    // 2. Record command buffer
    ws.cmd.begin();

    // Transition UNDEFINED -> COLOR_ATTACHMENT_OPTIMAL
    transition_image(raw_cmd, image,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, 0,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);

    // Begin dynamic rendering with clear color
    VkRenderingAttachmentInfo color_att{};
    color_att.sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    color_att.imageView   = ws.swapchain.current_image_view();
    color_att.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    color_att.loadOp      = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_att.storeOp     = VK_ATTACHMENT_STORE_OP_STORE;
    color_att.clearValue.color = {{
        ws.clear_color[0], ws.clear_color[1],
        ws.clear_color[2], ws.clear_color[3]
    }};

    VkRenderingInfo rendering{};
    rendering.sType                = VK_STRUCTURE_TYPE_RENDERING_INFO;
    rendering.renderArea           = {{0, 0}, {ws.swapchain.width(), ws.swapchain.height()}};
    rendering.layerCount           = 1;
    rendering.colorAttachmentCount = 1;
    rendering.pColorAttachments    = &color_att;

    vkCmdBeginRendering(raw_cmd, &rendering);
    vkCmdEndRendering(raw_cmd);

    // Transition COLOR_ATTACHMENT_OPTIMAL -> PRESENT_SRC_KHR
    transition_image(raw_cmd, image,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, 0);

    ws.cmd.end();

    // 3. Submit
    SubmitInfo submit{};
    submit.wait_semaphore   = ws.swapchain.image_available_semaphore();
    submit.signal_semaphore = ws.swapchain.render_finished_semaphore();
    submit.fence            = ws.swapchain.in_flight_fence();
    device.submit(ws.cmd, submit);

    // 4. Present
    ws.swapchain.present();

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

    // --- Create two windows via the Windows manager ---
    Window primary_win(WindowDesc{
        .title  = "Helios Sandbox - Primary (Blue)",
        .width  = 1280,
        .height = 720,
    });
    Window secondary_win(WindowDesc{
        .title  = "Helios Sandbox - Secondary (Red)",
        .width  = 640,
        .height = 480,
    });

    HELIOS_LOG(Core, Info, "Primary window:   {}x{}", primary_win.width(), primary_win.height());
    HELIOS_LOG(Core, Info, "Secondary window:  {}x{}", secondary_win.width(), secondary_win.height());

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

    // --- Vulkan context + device ---
    VulkanContext context("HeliosSandbox", /*enable_validation=*/true);
    if (!context) {
        HELIOS_LOG(Core, Error, "Failed to create Vulkan context");
        return 1;
    }

    auto* primary_glfw = static_cast<GLFWwindow*>(primary_win.native_handle());
    VkSurfaceKHR primary_surface = context.create_surface(primary_glfw);
    if (primary_surface == VK_NULL_HANDLE) {
        HELIOS_LOG(Core, Error, "Failed to create primary Vulkan surface");
        return 1;
    }

    VulkanDevice device(context, primary_surface);
    if (!device) {
        HELIOS_LOG(Core, Error, "Failed to create Vulkan device");
        return 1;
    }

    // --- Per-window render state ---
    // Primary window (dark blue)
    WindowRenderState primary_ws;
    primary_ws.surface       = primary_surface;
    primary_ws.clear_color[0] = 0.1f;
    primary_ws.clear_color[1] = 0.1f;
    primary_ws.clear_color[2] = 0.3f;
    primary_ws.clear_color[3] = 1.0f;
    primary_ws.label         = "Primary";
    primary_ws.swapchain     = VulkanSwapchain(device, SwapchainDesc{
        .width   = primary_win.width(),
        .height  = primary_win.height(),
        .surface = primary_surface,
    });
    primary_ws.cmd = VulkanCommandBuffer(device);

    // Secondary window (dark red)
    auto* secondary_glfw = static_cast<GLFWwindow*>(secondary_win.native_handle());
    VkSurfaceKHR secondary_surface = context.create_surface(secondary_glfw);
    if (secondary_surface == VK_NULL_HANDLE) {
        HELIOS_LOG(Core, Error, "Failed to create secondary Vulkan surface");
        return 1;
    }

    WindowRenderState secondary_ws;
    secondary_ws.surface       = secondary_surface;
    secondary_ws.clear_color[0] = 0.3f;
    secondary_ws.clear_color[1] = 0.1f;
    secondary_ws.clear_color[2] = 0.1f;
    secondary_ws.clear_color[3] = 1.0f;
    secondary_ws.label         = "Secondary";
    secondary_ws.swapchain     = VulkanSwapchain(device, SwapchainDesc{
        .width   = secondary_win.width(),
        .height  = secondary_win.height(),
        .surface = secondary_surface,
    });
    secondary_ws.cmd = VulkanCommandBuffer(device);

    HELIOS_LOG(Core, Info, "Vulkan initialized: context + device + 2 swapchains");
    HELIOS_LOG(Core, Info, "Starting render loop");
    HELIOS_LOG(Core, Info, "  Primary  = dark blue (0.1, 0.1, 0.3)");
    HELIOS_LOG(Core, Info, "  Secondary = dark red  (0.3, 0.1, 0.1)");
    HELIOS_LOG(Core, Info, "  Close primary to quit. Secondary can close independently.");

    bool secondary_alive = true;

    // --- Main loop: runs until primary window closes ---
    while (!primary_win.should_close()) {
        glfwPollEvents();

        // Always render to primary
        render_window(device, primary_ws);

        // Handle secondary window lifecycle
        if (secondary_alive) {
            if (secondary_win.should_close()) {
                HELIOS_LOG(Core, Info, "Secondary window closed — cleaning up its Vulkan resources");
                device.wait_idle();
                secondary_ws.cmd       = VulkanCommandBuffer{};
                secondary_ws.swapchain = VulkanSwapchain{};
                context.destroy_surface(secondary_surface);
                secondary_surface = VK_NULL_HANDLE;
                secondary_alive = false;
            } else {
                render_window(device, secondary_ws);
            }
        }
    }

    // --- Cleanup (reverse order: GPU resources before windows/context) ---
    HELIOS_LOG(Core, Info, "Primary window closed — shutting down");
    device.wait_idle();

    // Destroy per-window Vulkan state before device
    if (secondary_alive) {
        secondary_ws.cmd       = VulkanCommandBuffer{};
        secondary_ws.swapchain = VulkanSwapchain{};
        context.destroy_surface(secondary_surface);
    }

    primary_ws.cmd       = VulkanCommandBuffer{};
    primary_ws.swapchain = VulkanSwapchain{};
    // primary_surface lifetime managed by device/context

    // device, context, windows destroyed by RAII in stack order

    HELIOS_LOG(Core, Info, "=== Sandbox shutdown ===");
    return 0;
}
