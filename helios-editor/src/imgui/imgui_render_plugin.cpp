#include "imgui_render_plugin.h"
#include "../editor_state.h"

#include <helios/ecs/world.h>
#include <helios/ecs/system_params.h>
#include <helios/render_plugin.h>
#include <helios/window/windows.h>
#include <helios/core/log_macros.h>

// Vulkan backend escape hatch — editor-only includes
#include <helios/vulkan/vulkan_device.h>
#include <helios/vulkan/vulkan_context.h>
#include <helios/vulkan/vulkan_swapchain.h>
#include <helios/vulkan/vulkan_command_buffer.h>
#include <helios/vulkan/vulkan_texture.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <ImGuizmo.h>

#include <GLFW/glfw3.h>
#include <filesystem>

HELIOS_DECLARE_LOG_CHANNEL(Render);

namespace helios::editor {

static VkDescriptorPool s_imgui_pool = VK_NULL_HANDLE;

// ---- Startup system ----

static void init_imgui(
    ResMut<RenderContext> render_ctx,
    Res<Windows> windows,
    ResMut<ImGuiState> ctx)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Disable imgui.ini — dock layout is built programmatically
    io.IniFilename = nullptr;

    // -- Fonts --
    // Try both relative to cwd and relative to executable
    auto find_font = [](const char* name) -> std::string {
        std::string paths[] = {
            std::string("resources/Fonts/opensans/") + name,
            std::string("helios-editor/resources/Fonts/opensans/") + name,
        };
        for (auto& p : paths) {
            if (std::filesystem::exists(p)) return p;
        }
        return {};
    };

    auto bold_path = find_font("OpenSans-Bold.ttf");
    auto regular_path = find_font("OpenSans-Regular.ttf");

    if (!bold_path.empty()) {
        io.Fonts->AddFontFromFileTTF(bold_path.c_str(), 18.0f);
    }
    if (!regular_path.empty()) {
        io.FontDefault = io.Fonts->AddFontFromFileTTF(regular_path.c_str(), 18.0f);
    }

    // Merge Material Design Icons into the default font
    auto icon_path = find_font("../materialdesignicons-webfont.ttf");
    if (icon_path.empty()) {
        // Try alternate locations
        std::string icon_paths[] = {
            "resources/materialdesignicons-webfont.ttf",
            "helios-editor/resources/materialdesignicons-webfont.ttf",
        };
        for (auto& p : icon_paths) {
            if (std::filesystem::exists(p)) { icon_path = p; break; }
        }
    }
    if (!icon_path.empty()) {
        static const ImWchar icon_ranges[] = { 0xF68C, 0xF1D17, 0 };
        ImFontConfig icons_config;
        icons_config.MergeMode = true;
        icons_config.PixelSnapH = true;
        icons_config.GlyphMinAdvanceX = 18.0f;
        io.Fonts->AddFontFromFileTTF(icon_path.c_str(), 18.0f, &icons_config, icon_ranges);
    }

    // -- Theme (ported from old editor) --
    ImGui::StyleColorsDark();
    auto& colors = ImGui::GetStyle().Colors;
    colors[ImGuiCol_WindowBg]           = ImVec4{ 0.1f, 0.105f, 0.11f, 1.0f };
    colors[ImGuiCol_Header]             = ImVec4{ 0.2f, 0.205f, 0.21f, 1.0f };
    colors[ImGuiCol_HeaderHovered]      = ImVec4{ 0.3f, 0.305f, 0.31f, 1.0f };
    colors[ImGuiCol_HeaderActive]       = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };
    colors[ImGuiCol_Button]             = ImVec4{ 0.2f, 0.205f, 0.21f, 1.0f };
    colors[ImGuiCol_ButtonHovered]      = ImVec4{ 0.3f, 0.305f, 0.31f, 1.0f };
    colors[ImGuiCol_ButtonActive]       = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };
    colors[ImGuiCol_FrameBg]            = ImVec4{ 0.2f, 0.205f, 0.21f, 1.0f };
    colors[ImGuiCol_FrameBgHovered]     = ImVec4{ 0.3f, 0.305f, 0.31f, 1.0f };
    colors[ImGuiCol_FrameBgActive]      = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };
    colors[ImGuiCol_Tab]                = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };
    colors[ImGuiCol_TabHovered]         = ImVec4{ 0.38f, 0.3805f, 0.381f, 1.0f };
    colors[ImGuiCol_TabActive]          = ImVec4{ 0.28f, 0.2805f, 0.281f, 1.0f };
    colors[ImGuiCol_TabUnfocused]       = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4{ 0.2f, 0.205f, 0.21f, 1.0f };
    colors[ImGuiCol_TitleBg]            = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };
    colors[ImGuiCol_TitleBgActive]      = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };
    colors[ImGuiCol_TitleBgCollapsed]   = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };

    // RHI escape hatch: cast to VulkanDevice to get native handles
    auto* vk_device = dynamic_cast<rhi::vulkan::VulkanDevice*>(render_ctx->device.get());
    if (!vk_device) {
        HELIOS_LOG(Render, Error, "ImGui init failed: device is not VulkanDevice");
        return;
    }

    auto* glfw_window = static_cast<GLFWwindow*>(windows->primary().native_handle());
    ImGui_ImplGlfw_InitForVulkan(glfw_window, true);

    // Descriptor pool for ImGui
    VkDescriptorPoolSize pool_sizes[] = {
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 100 },
    };

    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 100;
    pool_info.poolSizeCount = 1;
    pool_info.pPoolSizes = pool_sizes;
    vkCreateDescriptorPool(vk_device->device(), &pool_info, nullptr, &s_imgui_pool);

    // Vulkan backend init with dynamic rendering
    auto* vk_swapchain = dynamic_cast<rhi::vulkan::VulkanSwapchain*>(render_ctx->swapchain.get());
    VkFormat color_format = vk_swapchain ? vk_swapchain->vk_format() : VK_FORMAT_B8G8R8A8_UNORM;

    ImGui_ImplVulkan_InitInfo init_info{};
    init_info.Instance = vk_device->context().instance();
    init_info.PhysicalDevice = vk_device->physical_device();
    init_info.Device = vk_device->device();
    init_info.QueueFamily = vk_device->graphics_queue_family();
    init_info.Queue = vk_device->graphics_queue();
    init_info.DescriptorPool = s_imgui_pool;
    init_info.MinImageCount = rhi::Device::MAX_FRAMES_IN_FLIGHT;
    init_info.ImageCount = rhi::Device::MAX_FRAMES_IN_FLIGHT;
    init_info.UseDynamicRendering = true;
    init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.PipelineInfoMain.PipelineRenderingCreateInfo.sType =
        VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
    init_info.PipelineInfoMain.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
    init_info.PipelineInfoMain.PipelineRenderingCreateInfo.pColorAttachmentFormats = &color_format;
    init_info.PipelineInfoMain.PipelineRenderingCreateInfo.depthAttachmentFormat = VK_FORMAT_D32_SFLOAT;

    ImGui_ImplVulkan_Init(&init_info);

    ctx->initialized = true;
    HELIOS_LOG(Render, Info, "ImGui initialized (Vulkan + GLFW, dynamic rendering)");
}

// ---- Per-frame: begin ImGui frame ----

static void begin_imgui_frame(Res<ImGuiState> ctx) {
    if (!ctx->initialized) return;
    // Swapchain rendering pass is already open (opened by frame_end in Offscreen mode)
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    ImGuizmo::BeginFrame();
}

// ---- Per-frame: end ImGui frame, render draw data ----

static void end_imgui_frame(Res<ImGuiState> ctx, ResMut<RenderContext> render_ctx) {
    if (!ctx->initialized) return;

    // Always call Render() to match NewFrame(), even if we can't submit to GPU
    ImGui::Render();

    if (!render_ctx->frame_active) return;

    ImDrawData* draw_data = ImGui::GetDrawData();
    if (draw_data) {
        auto* vk_cmd = dynamic_cast<rhi::vulkan::VulkanCommandBuffer*>(render_ctx->cmd);
        if (vk_cmd) {
            ImGui_ImplVulkan_RenderDrawData(draw_data, vk_cmd->vk_command_buffer());
        }
    }
}

// ---- Shutdown ----

static void shutdown_imgui(ResMut<ImGuiState> ctx, ResMut<RenderContext> render_ctx) {
    if (!ctx->initialized) return;

    render_ctx->device->wait_idle();

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    if (s_imgui_pool != VK_NULL_HANDLE) {
        auto* vk_device = dynamic_cast<rhi::vulkan::VulkanDevice*>(render_ctx->device.get());
        if (vk_device) {
            vkDestroyDescriptorPool(vk_device->device(), s_imgui_pool, nullptr);
        }
        s_imgui_pool = VK_NULL_HANDLE;
    }

    ctx->initialized = false;
}

// ---- Plugin ----

void ImGuiRenderPlugin::build(App& app) {
    app.insert_resource(ImGuiState{});

    app.add_system(Schedule::Startup, init_imgui, "init_imgui");

    // ImGui renders to swapchain pass, between frame_end and frame_present:
    // frame_end → begin_imgui_frame → [panels] → end_imgui_frame → frame_present
    auto render_end = app.id_of("frame_end");
    auto present_id = app.id_of("frame_present");

    auto begin_id = app.add_system(Schedule::PreRender, begin_imgui_frame,
                                    "begin_imgui_frame")
        .after(render_end).before(present_id).id();

    app.add_system(Schedule::PreRender, end_imgui_frame, "end_imgui_frame")
        .after(begin_id).before(present_id);

    app.add_system(Schedule::Shutdown, shutdown_imgui, "shutdown_imgui")
        .before(app.id_of("gpu_shutdown"));
}

// ---- Public texture helpers (hide Vulkan behind this API) ----

ImTextureID imgui_register_texture(rhi::Texture& texture) {
    auto* vk_tex = dynamic_cast<rhi::vulkan::VulkanTexture*>(&texture);
    if (!vk_tex) return ImTextureID{};
    VkDescriptorSet ds = ImGui_ImplVulkan_AddTexture(
        vk_tex->vk_sampler(), vk_tex->vk_image_view(),
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    return reinterpret_cast<ImTextureID>(ds);
}

void imgui_unregister_texture(ImTextureID id) {
    if (id) {
        ImGui_ImplVulkan_RemoveTexture(reinterpret_cast<VkDescriptorSet>(id));
    }
}

} // namespace helios::editor
