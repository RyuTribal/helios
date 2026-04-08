// Helios Engine - PBR Sandbox Demo
// Renders the DamagedHelmet glTF model with PBR textures and HDR skybox.
// Uses ONLY abstract RHI interfaces -- no backend-specific headers.

#include <helios/ecs/ecs.h>
#include <helios/components/components.h>
#include <helios/core/logging.h>
#include <helios/window/window_plugin.h>
#include <helios/window/window_events.h>
#include <helios/input/input_plugin.h>
#include <helios/input/input_map.h>
#include <helios/input/raw_input.h>
#include <helios/render_plugin.h>
#include <helios/forward_plus/forward_plus_plugin.h>
#include <helios/forward_plus/simple_render_state.h>
#include <helios/forward_plus/gpu_data.h>
#include <helios/graph/frame_packet.h>

#include "asset_loader.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <vector>

using namespace helios;

// ============================================================
// Log channels
// ============================================================

HELIOS_DEFINE_LOG_CHANNEL(Game);
HELIOS_DEFINE_LOG_CHANNEL(Scene);

// ============================================================
// Helper: read SPIR-V file from disk
// ============================================================

static std::vector<uint8_t> read_spirv(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return {};
    auto sz = file.tellg();
    std::vector<uint8_t> data(static_cast<size_t>(sz));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(data.data()), sz);
    return data;
}

// ============================================================
// Skybox cube geometry (36 vertices, position only)
// ============================================================

static std::vector<glm::vec3> build_skybox_cube() {
    return {
        // +Z face
        {-1, -1,  1}, { 1, -1,  1}, { 1,  1,  1},
        { 1,  1,  1}, {-1,  1,  1}, {-1, -1,  1},
        // -Z face
        { 1, -1, -1}, {-1, -1, -1}, {-1,  1, -1},
        {-1,  1, -1}, { 1,  1, -1}, { 1, -1, -1},
        // +X face
        { 1, -1,  1}, { 1, -1, -1}, { 1,  1, -1},
        { 1,  1, -1}, { 1,  1,  1}, { 1, -1,  1},
        // -X face
        {-1, -1, -1}, {-1, -1,  1}, {-1,  1,  1},
        {-1,  1,  1}, {-1,  1, -1}, {-1, -1, -1},
        // +Y face
        {-1,  1,  1}, { 1,  1,  1}, { 1,  1, -1},
        { 1,  1, -1}, {-1,  1, -1}, {-1,  1,  1},
        // -Y face
        {-1, -1, -1}, { 1, -1, -1}, { 1, -1,  1},
        { 1, -1,  1}, {-1, -1,  1}, {-1, -1, -1},
    };
}

// ============================================================
// Systems
// ============================================================

void log_frame_packet(Res<renderer::FramePacket> packet, Res<Time> time) {
    if (time->frame_count() % 300 == 0 && time->frame_count() > 0) {
        HELIOS_LOG(Game, Debug, "Frame {} | FramePacket: {} meshes, {} dir lights | dt={:.3f}ms",
            time->frame_count(),
            packet->mesh_draws.size(),
            packet->dir_lights.size(),
            time->delta() * 1000.0f);
    }
}

void handle_resize(EventReader<WindowResized> events) {
    for (const auto& e : events) {
        HELIOS_LOG(Game, Debug, "Window resized: {}x{}", e.width, e.height);
    }
}

// ============================================================
// Plugins
// ============================================================

struct GamePlugin {
    void build(App& app) {
        app.add_system(Schedule::PostUpdate, log_frame_packet, "log_frame_packet");
        app.add_system(Schedule::PreUpdate, handle_resize, "handle_resize");
        HELIOS_LOG(Game, Info, "GamePlugin initialized");
    }
};

struct ScenePlugin {
    void build(App& app) {
        auto& world = app.world();

        // Camera -- looking at the helmet from the front
        world.spawn(
            Transform{ .position = glm::vec3{0.0f, 0.0f, 3.0f} },
            Camera{ .fov_degrees = 60.0f, .near_plane = 0.1f, .far_plane = 100.0f },
            ActiveCamera{},
            Tag{ .name = "main_camera" });

        // Directional light (sun)
        world.spawn(
            Transform{ .position = glm::vec3{0, 50, 0},
                        .rotation = glm::quat(glm::vec3(glm::radians(-45.0f), glm::radians(30.0f), 0.0f)) },
            DirectionalLight{ .color = glm::vec3{1.0f, 0.95f, 0.8f}, .intensity = 3.0f },
            Tag{ .name = "sun" });

        // Helmet mesh entity
        world.spawn(
            Transform{ .position = glm::vec3{0.0f, 0.0f, 0.0f},
                        .rotation = glm::quat(glm::vec3(0.0f, glm::radians(180.0f), 0.0f)) },
            MeshRenderer{ .mesh = AssetHandle{1}, .material = AssetHandle{1} },
            Tag{ .name = "damaged_helmet" });

        HELIOS_LOG(Scene, Info, "Scene: camera + sun + damaged_helmet");
    }
};

// ============================================================
// SandboxAssetsPlugin: loads all GPU resources after RenderPlugin init
// ============================================================

struct SandboxAssetsPlugin {
    void build(App& app) {
        auto& ctx = app.world().resource<RenderContext>();
        auto& device = *ctx.device;

        const std::string asset_dir = HELIOS_DEMO_ASSET_DIR;
        const std::string helmet_dir = asset_dir + "/Meshes/damaged_helmet_source_glb";
        const std::string tex_dir = helmet_dir + "/textures";

#ifdef HELIOS_SHADER_DIR
        const std::string shader_dir = HELIOS_SHADER_DIR;
#else
        const std::string shader_dir = "shaders";
#endif

        SimpleRenderState state;

        // ---- Load helmet mesh ----
        auto helmet = sandbox::load_gltf_mesh(device,
            (helmet_dir + "/scene.gltf").c_str());
        if (!helmet.vbo || !helmet.ibo) {
            HELIOS_LOG(Game, Error, "Failed to load helmet mesh");
            app.insert_resource(std::move(state));
            return;
        }
        state.mesh_vbo = std::move(helmet.vbo);
        state.mesh_ibo = std::move(helmet.ibo);
        state.index_count = helmet.index_count;

        // ---- Load PBR textures ----
        state.albedo_tex = sandbox::load_texture_2d(device,
            (tex_dir + "/Material_MR_baseColor.jpeg").c_str(), "HelmetAlbedo");
        state.normal_tex = sandbox::load_texture_2d(device,
            (tex_dir + "/Material_MR_normal.jpeg").c_str(), "HelmetNormal");
        state.metallic_roughness_tex = sandbox::load_texture_2d(device,
            (tex_dir + "/Material_MR_metallicRoughness.png").c_str(), "HelmetMR");
        state.emissive_tex = sandbox::load_texture_2d(device,
            (tex_dir + "/Material_MR_emissive.jpeg").c_str(), "HelmetEmissive");

        if (!state.albedo_tex || !state.normal_tex ||
            !state.metallic_roughness_tex || !state.emissive_tex) {
            HELIOS_LOG(Game, Error, "Failed to load one or more PBR textures");
            app.insert_resource(std::move(state));
            return;
        }

        // ---- Load HDR skybox and convert to cubemap ----
        auto equirect = sandbox::load_hdr_texture(device,
            (asset_dir + "/Textures/default_skybox.hdr").c_str(), "SkyboxEquirect");
        if (equirect) {
            state.env_cubemap = sandbox::convert_equirect_to_cubemap(
                device, *ctx.cmd, *equirect, 1024);
        }

        // ---- Create depth buffer ----
        {
            rhi::TextureDesc depth_desc;
            depth_desc.width = ctx.swapchain->width();
            depth_desc.height = ctx.swapchain->height();
            depth_desc.format = rhi::TextureFormat::Depth32F;
            depth_desc.type = rhi::TextureType::Texture2D;
            depth_desc.mip_levels = 1;
            depth_desc.array_layers = 1;
            depth_desc.usage = rhi::TextureUsage::DepthAttachment;
            depth_desc.debug_name = "DepthBuffer";
            state.depth_texture = device.create_texture(depth_desc);
        }

        // ---- PBR Camera UBO (set 0, binding 0) ----
        {
            rhi::BufferDesc ubo_desc;
            ubo_desc.size = sizeof(PBRCameraUBO);
            ubo_desc.usage = rhi::BufferUsage::Uniform;
            ubo_desc.access = rhi::MemoryAccess::CPU_to_GPU;
            ubo_desc.debug_name = "PBRCameraUBO";
            state.camera_ubo = device.create_buffer(ubo_desc);
        }

        // ---- Camera descriptor set layout (set 0) ----
        {
            rhi::DescriptorSetLayoutDesc layout_desc;
            layout_desc.bindings = {
                rhi::DescriptorBinding{
                    .binding = 0,
                    .type = rhi::DescriptorType::UniformBuffer,
                    .stage = rhi::ShaderStage::Vertex | rhi::ShaderStage::Fragment,
                    .count = 1,
                },
            };
            layout_desc.debug_name = "PBRCamera_DSL";
            state.camera_layout = device.create_descriptor_set_layout(layout_desc);
        }

        // ---- Material descriptor set layout (set 1) ----
        {
            rhi::DescriptorSetLayoutDesc layout_desc;
            layout_desc.bindings = {
                rhi::DescriptorBinding{
                    .binding = 0,
                    .type = rhi::DescriptorType::CombinedImageSampler,
                    .stage = rhi::ShaderStage::Fragment,
                    .count = 1,
                },
                rhi::DescriptorBinding{
                    .binding = 1,
                    .type = rhi::DescriptorType::CombinedImageSampler,
                    .stage = rhi::ShaderStage::Fragment,
                    .count = 1,
                },
                rhi::DescriptorBinding{
                    .binding = 2,
                    .type = rhi::DescriptorType::CombinedImageSampler,
                    .stage = rhi::ShaderStage::Fragment,
                    .count = 1,
                },
                rhi::DescriptorBinding{
                    .binding = 3,
                    .type = rhi::DescriptorType::CombinedImageSampler,
                    .stage = rhi::ShaderStage::Fragment,
                    .count = 1,
                },
                rhi::DescriptorBinding{
                    .binding = 4,
                    .type = rhi::DescriptorType::CombinedImageSampler,
                    .stage = rhi::ShaderStage::Fragment,
                    .count = 1,
                },
            };
            layout_desc.debug_name = "PBRMaterial_DSL";
            state.material_layout = device.create_descriptor_set_layout(layout_desc);
        }

        // ---- Load PBR shaders ----
        {
            auto vert_spirv = read_spirv(std::filesystem::path(shader_dir) / "pbr_simple.vert.spv");
            auto frag_spirv = read_spirv(std::filesystem::path(shader_dir) / "pbr_simple.frag.spv");
            if (vert_spirv.empty() || frag_spirv.empty()) {
                HELIOS_LOG(Game, Error, "Failed to load PBR shaders from '{}'", shader_dir);
                app.insert_resource(std::move(state));
                return;
            }

            rhi::ShaderDesc vert_desc;
            vert_desc.stage = rhi::ShaderStage::Vertex;
            vert_desc.spirv_code = std::move(vert_spirv);
            vert_desc.entry_point = "main";
            vert_desc.debug_name = "pbr_simple_vert";
            state.vert_shader = device.create_shader(vert_desc);

            rhi::ShaderDesc frag_desc;
            frag_desc.stage = rhi::ShaderStage::Fragment;
            frag_desc.spirv_code = std::move(frag_spirv);
            frag_desc.entry_point = "main";
            frag_desc.debug_name = "pbr_simple_frag";
            state.frag_shader = device.create_shader(frag_desc);
        }

        // ---- PBR graphics pipeline ----
        {
            rhi::GraphicsPipelineDesc pipe_desc;
            pipe_desc.vertex_shader = state.vert_shader.get();
            pipe_desc.fragment_shader = state.frag_shader.get();
            pipe_desc.layout.stride = sizeof(sandbox::PBRVertex);
            pipe_desc.layout.attributes = {
                rhi::VertexAttribute{
                    .location = 0, .binding = 0,
                    .offset = 0,
                    .format = rhi::TextureFormat::RGB32F,  // position
                },
                rhi::VertexAttribute{
                    .location = 1, .binding = 0,
                    .offset = sizeof(glm::vec3),
                    .format = rhi::TextureFormat::RGB32F,  // normal
                },
                rhi::VertexAttribute{
                    .location = 2, .binding = 0,
                    .offset = sizeof(glm::vec3) * 2,
                    .format = rhi::TextureFormat::RG32F,   // uv
                },
                rhi::VertexAttribute{
                    .location = 3, .binding = 0,
                    .offset = sizeof(glm::vec3) * 2 + sizeof(glm::vec2),
                    .format = rhi::TextureFormat::RGBA32F, // tangent
                },
            };
            pipe_desc.state.cull = rhi::CullMode::Back;
            pipe_desc.state.depth = rhi::DepthCompare::Less;
            pipe_desc.state.depth_test = true;
            pipe_desc.state.depth_write = true;
            pipe_desc.state.blend = rhi::BlendMode::None;
            pipe_desc.render_pass = nullptr;
            pipe_desc.descriptor_layouts = {
                state.camera_layout.get(),
                state.material_layout.get()
            };
            pipe_desc.push_constant_size = sizeof(PushConstantData);
            pipe_desc.push_constant_stages = rhi::ShaderStage::Vertex;
            pipe_desc.debug_name = "PBRSimple";
            pipe_desc.use_dynamic_rendering = true;
            pipe_desc.dynamic_color_formats = { rhi::TextureFormat::BGRA8 };
            pipe_desc.dynamic_depth_format = rhi::TextureFormat::Depth32F;

            state.pipeline = device.create_graphics_pipeline(pipe_desc);
            if (!state.pipeline) {
                HELIOS_LOG(Game, Error, "Failed to create PBR pipeline");
                app.insert_resource(std::move(state));
                return;
            }
        }

        // ---- Camera descriptor set ----
        {
            state.camera_ds = device.allocate_descriptor_set(*state.camera_layout);
            device.update_descriptor_set(*state.camera_ds, {
                rhi::DescriptorWrite{
                    .binding = 0,
                    .type = rhi::DescriptorType::UniformBuffer,
                    .buffer_handle = state.camera_ubo.get(),
                    .range = sizeof(PBRCameraUBO),
                },
            });
        }

        // ---- Material descriptor set ----
        {
            state.material_ds = device.allocate_descriptor_set(*state.material_layout);

            // Use the env cubemap for binding 4, or albedo as fallback
            rhi::Texture* env_tex = state.env_cubemap ? state.env_cubemap.get() : state.albedo_tex.get();

            device.update_descriptor_set(*state.material_ds, {
                rhi::DescriptorWrite{
                    .binding = 0,
                    .type = rhi::DescriptorType::CombinedImageSampler,
                    .texture_handle = state.albedo_tex.get(),
                },
                rhi::DescriptorWrite{
                    .binding = 1,
                    .type = rhi::DescriptorType::CombinedImageSampler,
                    .texture_handle = state.normal_tex.get(),
                },
                rhi::DescriptorWrite{
                    .binding = 2,
                    .type = rhi::DescriptorType::CombinedImageSampler,
                    .texture_handle = state.metallic_roughness_tex.get(),
                },
                rhi::DescriptorWrite{
                    .binding = 3,
                    .type = rhi::DescriptorType::CombinedImageSampler,
                    .texture_handle = state.emissive_tex.get(),
                },
                rhi::DescriptorWrite{
                    .binding = 4,
                    .type = rhi::DescriptorType::CombinedImageSampler,
                    .texture_handle = env_tex,
                },
            });
        }

        state.valid = true;
        HELIOS_LOG(Game, Info, "PBR pipeline created: {} indices", state.index_count);

        // ============================================================
        // Skybox setup
        // ============================================================
        if (state.env_cubemap) {
            // Load skybox shaders
            auto sky_vert_spirv = read_spirv(std::filesystem::path(shader_dir) / "skybox.vert.spv");
            auto sky_frag_spirv = read_spirv(std::filesystem::path(shader_dir) / "skybox.frag.spv");

            if (!sky_vert_spirv.empty() && !sky_frag_spirv.empty()) {
                rhi::ShaderDesc sv_desc;
                sv_desc.stage = rhi::ShaderStage::Vertex;
                sv_desc.spirv_code = std::move(sky_vert_spirv);
                sv_desc.entry_point = "main";
                sv_desc.debug_name = "skybox_vert";
                state.skybox_vert = device.create_shader(sv_desc);

                rhi::ShaderDesc sf_desc;
                sf_desc.stage = rhi::ShaderStage::Fragment;
                sf_desc.spirv_code = std::move(sky_frag_spirv);
                sf_desc.entry_point = "main";
                sf_desc.debug_name = "skybox_frag";
                state.skybox_frag = device.create_shader(sf_desc);

                // Skybox descriptor layout: UBO at binding 0, cubemap at binding 1
                rhi::DescriptorSetLayoutDesc sky_layout_desc;
                sky_layout_desc.bindings = {
                    rhi::DescriptorBinding{
                        .binding = 0,
                        .type = rhi::DescriptorType::UniformBuffer,
                        .stage = rhi::ShaderStage::Vertex | rhi::ShaderStage::Fragment,
                        .count = 1,
                    },
                    rhi::DescriptorBinding{
                        .binding = 1,
                        .type = rhi::DescriptorType::CombinedImageSampler,
                        .stage = rhi::ShaderStage::Fragment,
                        .count = 1,
                    },
                };
                sky_layout_desc.debug_name = "Skybox_DSL";
                state.skybox_layout = device.create_descriptor_set_layout(sky_layout_desc);

                // Skybox UBO
                rhi::BufferDesc sky_ubo_desc;
                sky_ubo_desc.size = sizeof(SkyboxUBOData);
                sky_ubo_desc.usage = rhi::BufferUsage::Uniform;
                sky_ubo_desc.access = rhi::MemoryAccess::CPU_to_GPU;
                sky_ubo_desc.debug_name = "SkyboxUBO";
                state.skybox_ubo = device.create_buffer(sky_ubo_desc);

                // Skybox pipeline: depth test enabled, depth write disabled, LessEqual
                rhi::GraphicsPipelineDesc sky_pipe;
                sky_pipe.vertex_shader = state.skybox_vert.get();
                sky_pipe.fragment_shader = state.skybox_frag.get();
                sky_pipe.layout.stride = sizeof(glm::vec3);
                sky_pipe.layout.attributes = {
                    rhi::VertexAttribute{
                        .location = 0, .binding = 0, .offset = 0,
                        .format = rhi::TextureFormat::RGB32F,
                    },
                };
                sky_pipe.state.cull = rhi::CullMode::None;
                sky_pipe.state.depth = rhi::DepthCompare::LessEqual;
                sky_pipe.state.depth_test = true;
                sky_pipe.state.depth_write = false;
                sky_pipe.state.blend = rhi::BlendMode::None;
                sky_pipe.render_pass = nullptr;
                sky_pipe.descriptor_layouts = { state.skybox_layout.get() };
                sky_pipe.push_constant_size = 0;
                sky_pipe.debug_name = "SkyboxPipeline";
                sky_pipe.use_dynamic_rendering = true;
                sky_pipe.dynamic_color_formats = { rhi::TextureFormat::BGRA8 };
                sky_pipe.dynamic_depth_format = rhi::TextureFormat::Depth32F;

                state.skybox_pipeline = device.create_graphics_pipeline(sky_pipe);

                // Skybox cube VBO
                auto sky_verts = build_skybox_cube();
                rhi::BufferDesc sky_vbo_desc;
                sky_vbo_desc.size = static_cast<uint32_t>(sky_verts.size() * sizeof(glm::vec3));
                sky_vbo_desc.usage = rhi::BufferUsage::Vertex;
                sky_vbo_desc.access = rhi::MemoryAccess::CPU_to_GPU;
                sky_vbo_desc.debug_name = "SkyboxCubeVBO";
                state.skybox_cube_vbo = device.create_buffer(sky_vbo_desc, sky_verts.data());

                // Skybox descriptor set
                state.skybox_ds = device.allocate_descriptor_set(*state.skybox_layout);
                device.update_descriptor_set(*state.skybox_ds, {
                    rhi::DescriptorWrite{
                        .binding = 0,
                        .type = rhi::DescriptorType::UniformBuffer,
                        .buffer_handle = state.skybox_ubo.get(),
                        .range = sizeof(SkyboxUBOData),
                    },
                    rhi::DescriptorWrite{
                        .binding = 1,
                        .type = rhi::DescriptorType::CombinedImageSampler,
                        .texture_handle = state.env_cubemap.get(),
                    },
                });

                state.has_skybox = true;
                HELIOS_LOG(Game, Info, "Skybox pipeline created");
            } else {
                HELIOS_LOG(Game, Warn, "Skybox shaders not found, skybox disabled");
            }
        }

        app.insert_resource(std::move(state));
        HELIOS_LOG(Game, Info, "SandboxAssetsPlugin: all GPU resources loaded");
    }
};

// ============================================================
// Main
// ============================================================

int main() {
    LogSystem log(LogConfig{
        .enable_file_sink = false,
        .default_level = LogLevel::Debug,
    });

    HELIOS_LOG(Core, Info, "=== Helios PBR Sandbox ===");

    App app;

    // Engine plugins (order matters: Window -> Render -> ForwardPlus)
    app.add_plugin(WindowPlugin{ .primary_window = WindowDesc{
        .title = "Helios PBR Sandbox",
        .width = 1280,
        .height = 720,
    }});
    app.add_plugin(InputPlugin{});
    app.add_plugin(RenderPlugin{});
    app.add_plugin(ForwardPlusPlugin{});

    // Asset loading (must be after RenderPlugin and before game systems)
    app.add_plugin(SandboxAssetsPlugin{});

    // Game plugins
    app.add_plugin(GamePlugin{});
    app.add_plugin(ScenePlugin{});

    HELIOS_LOG(Core, Info, "All plugins loaded. Starting engine...");

    app.run();

    HELIOS_LOG(Core, Info, "=== Sandbox shutdown ===");
    return 0;
}
