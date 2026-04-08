// Helios Engine - PBR Sandbox Demo (3-state version)
//
// Demonstrates the asset-to-GPU pipeline. Users just load a mesh and
// spawn an entity -- the engine handles textures, materials, GPU upload.
//
// Three states:
//   Loading  -- loads assets for the next scene via AssetServer
//   Scene1   -- DamagedHelmet + skybox + physics
//   Scene2   -- Lion + skybox
//
// Press 1 to switch to Scene1, 2 to switch to Scene2.

#include <helios/ecs/ecs.h>
#include <helios/components/components.h>
#include <helios/core/logging.h>
#include <helios/window/window_plugin.h>
#include <helios/input/input_plugin.h>
#include <helios/input/input_map.h>
#include <helios/input/raw_input.h>
#include <helios/render_plugin.h>
#include <helios/forward_plus/forward_plus_plugin.h>
#include <helios/forward_plus/pbr_render_state.h>
#include <helios/forward_plus/skybox_state.h>
#include <helios/forward_plus/gpu_data.h>
#include <helios/forward_plus/gpu_cache.h>
#include <helios/graph/frame_packet.h>
#include <helios/app/game_flow_plugin.h>
#include <helios/app/state.h>
#include <helios/app/state_builder.h>
#include <helios/assets/asset_server.h>
#include <helios/assets/asset_plugin.h>
#include <helios/assets/mesh_asset.h>
#include <helios/assets/material_asset.h>
#include <helios/assets/handle.h>

#include "asset_loader.h"

// Physics
#include "interface/physics_world.h"
#include "interface/body_types.h"
#include "interface/physics_factory.h"

// Audio
#include "interface/audio_device.h"
#include "interface/audio_types.h"
#include "interface/audio_factory.h"

#include <cmath>
#include <cstring>
#include <vector>

using namespace helios;

// ============================================================
// Log channels
// ============================================================

HELIOS_DEFINE_LOG_CHANNEL(Game);
HELIOS_DEFINE_LOG_CHANNEL(Scene);
HELIOS_DEFINE_LOG_CHANNEL(Physics);
HELIOS_DEFINE_LOG_CHANNEL(Audio);

// ============================================================
// PhysicsDemo resource (bundles physics + audio state)
// ============================================================

struct PhysicsDemo {
    std::unique_ptr<physics::PhysicsWorld> physics;
    std::unique_ptr<audio::AudioDevice>    audio;
    physics::BodyHandle floor_body  = 0;
    physics::BodyHandle helmet_body = 0;
    std::vector<uint8_t> bounce_wav;
    bool active = false;
};

// ============================================================
// WAV generation: a short 220Hz sine "thud" with linear fade-out
// ============================================================

static std::vector<uint8_t> generate_bounce_wav() {
    constexpr int   sample_rate = 44100;
    constexpr float duration    = 0.1f;
    constexpr float freq        = 220.0f;
    const int num_samples = static_cast<int>(sample_rate * duration);

    std::vector<int16_t> samples(num_samples);
    for (int i = 0; i < num_samples; i++) {
        float t = static_cast<float>(i) / sample_rate;
        float envelope = 1.0f - (t / duration);
        float sample = std::sin(2.0f * 3.14159265f * freq * t) * envelope;
        samples[i] = static_cast<int16_t>(sample * 32767.0f * 0.5f);
    }

    uint32_t data_size = static_cast<uint32_t>(num_samples * sizeof(int16_t));
    uint32_t file_size = 36 + data_size;

    std::vector<uint8_t> wav;
    wav.resize(44 + data_size);
    auto write = [&](size_t off, const void* data, size_t n) {
        std::memcpy(wav.data() + off, data, n);
    };
    // RIFF header
    write(0, "RIFF", 4);
    write(4, &file_size, 4);
    write(8, "WAVE", 4);
    // fmt chunk
    write(12, "fmt ", 4);
    uint32_t fmt_size = 16;  write(16, &fmt_size, 4);
    uint16_t audio_fmt = 1;  write(20, &audio_fmt, 2);  // PCM
    uint16_t channels = 1;   write(22, &channels, 2);
    uint32_t sr = sample_rate; write(24, &sr, 4);
    uint32_t byte_rate = sample_rate * 2; write(28, &byte_rate, 4);
    uint16_t block_align = 2; write(32, &block_align, 2);
    uint16_t bits = 16;       write(34, &bits, 2);
    // data chunk
    write(36, "data", 4);
    write(40, &data_size, 4);
    std::memcpy(wav.data() + 44, samples.data(), data_size);

    return wav;
}

// ============================================================
// State enum
// ============================================================

enum class SceneState { Loading, Scene1, Scene2 };

/// Which scene we want to transition to next (set by input handling).
struct PendingScene {
    SceneState target = SceneState::Scene1;
    bool pending = false;
};

// ============================================================
// Orbit camera state (stored as a resource)
// ============================================================

struct OrbitCamera {
    float yaw   = 0.0f;       // radians
    float pitch = 0.0f;       // radians
    float distance = 3.0f;
    glm::vec3 target = {0.0f, 0.0f, 0.0f};
    float sensitivity = 0.003f;
    float zoom_speed  = 0.3f;
    bool panning = false;
};

// ============================================================
// Systems
// ============================================================

void orbit_camera_system(Res<RawInput> input,
                         ResMut<Windows> windows,
                         ResMut<OrbitCamera> orbit,
                         Query<Transform, With<ActiveCamera>> cameras)
{
    if (!windows->has_primary()) return;
    auto& win = windows->primary();

    bool rmb = input->mouse_button_pressed(MouseButton::Right);

    // Transition: start panning
    if (rmb && !orbit->panning) {
        orbit->panning = true;
        win.set_cursor_mode(Window::CursorMode::Captured);
    }
    // Transition: stop panning
    if (!rmb && orbit->panning) {
        orbit->panning = false;
        win.set_cursor_mode(Window::CursorMode::Normal);
    }

    // Apply mouse delta while panning
    if (orbit->panning) {
        glm::vec2 delta = input->mouse_delta();
        orbit->yaw   += delta.x * orbit->sensitivity;
        orbit->pitch -= delta.y * orbit->sensitivity;

        constexpr float max_pitch = glm::radians(89.0f);
        orbit->pitch = glm::clamp(orbit->pitch, -max_pitch, max_pitch);
    }

    // Scroll zoom (always active)
    float scroll = input->scroll_delta();
    if (scroll != 0.0f) {
        orbit->distance -= scroll * orbit->zoom_speed;
        orbit->distance = glm::clamp(orbit->distance, 0.5f, 20.0f);
    }

    // Compute camera position from spherical coordinates
    glm::vec3 offset;
    offset.x = orbit->distance * std::cos(orbit->pitch) * std::sin(orbit->yaw);
    offset.y = orbit->distance * std::sin(orbit->pitch);
    offset.z = orbit->distance * std::cos(orbit->pitch) * std::cos(orbit->yaw);

    glm::vec3 cam_pos = orbit->target + offset;

    for (auto [t] : cameras) {
        t.position = cam_pos;
        glm::mat4 look = glm::lookAt(cam_pos, orbit->target, glm::vec3(0, 1, 0));
        t.rotation = glm::conjugate(glm::quat_cast(look));
    }
}


// ============================================================
// Physics update system (runs during Update when demo is active)
// ============================================================

void physics_update_system(ResMut<PhysicsDemo> demo,
                           Res<Time> time,
                           Res<RawInput> input,
                           Query<Transform, const Tag> tagged) {
    if (!demo->active || !demo->physics) return;

    // R key: reset helmet to starting position
    if (input->key_just_pressed(KeyCode::R)) {
        HELIOS_LOG(Physics, Info, "Resetting helmet to starting position");
        demo->physics->destroy_body(demo->helmet_body);

        physics::BodyDesc helmet_desc;
        helmet_desc.type        = physics::BodyType::Dynamic;
        helmet_desc.position    = {0.0f, 3.0f, 0.0f};
        helmet_desc.shape       = physics::SphereShape{1.0f};
        helmet_desc.mass        = 2.0f;
        helmet_desc.restitution = 0.6f;
        demo->helmet_body = demo->physics->create_body(helmet_desc, 2);
    }

    // Step the simulation
    demo->physics->step(time->delta());

    // Read back helmet position and apply to the entity Transform
    auto pos = demo->physics->get_position(demo->helmet_body);
    for (auto [t, tag] : tagged) {
        if (tag.name == "damaged_helmet") {
            t.position = pos;
            // Keep the display rotation from the original spawn
        }
    }

    // Drain contact events and play bounce sounds
    auto contacts = demo->physics->drain_contacts();
    for (auto& contact : contacts) {
        HELIOS_LOG(Physics, Debug, "Contact at ({:.2f}, {:.2f}, {:.2f}) impulse={:.2f}",
                   contact.world_point.x, contact.world_point.y, contact.world_point.z,
                   contact.impulse);

        if (demo->audio && !demo->bounce_wav.empty()) {
            demo->audio->play_at(
                demo->bounce_wav.data(), demo->bounce_wav.size(),
                contact.world_point);
        }
    }

    if (demo->audio) {
        demo->audio->update();
    }
}

// ============================================================
// Forward declarations of states
// ============================================================

class LoadingState;
class Scene1State;
class Scene2State;

// ============================================================
// LoadingState -- loads assets via AssetServer, then transitions
// ============================================================

class LoadingState : public State<SceneState> {
public:
    explicit LoadingState(World& world) : m_world(&world) {
        auto& pending = world.resource<PendingScene>();
        m_target = pending.target;
        pending.pending = false;

        HELIOS_LOG(Scene, Info, "Loading assets for {}...",
                   m_target == SceneState::Scene1 ? "Scene1 (Helmet)" : "Scene2 (Lion)");

        auto& server = *world.resource<std::shared_ptr<AssetServer>>();

        // Load mesh asset synchronously (importer auto-loads textures + materials)
        if (m_target == SceneState::Scene1) {
            m_mesh_handle = server.load_sync<MeshAsset>(
                "Meshes/damaged_helmet_source_glb/scene.gltf");
        } else {
            m_mesh_handle = server.load_sync<MeshAsset>(
                "Meshes/lion/scene.gltf");
        }

        if (m_mesh_handle) {
            server.acquire(m_mesh_handle);
            HELIOS_LOG(Scene, Info, "Loaded mesh asset (handle {}/{})",
                       m_mesh_handle.index, m_mesh_handle.generation);
        } else {
            HELIOS_LOG(Scene, Error, "Failed to load mesh asset");
        }

        m_loaded = true;
        HELIOS_LOG(Scene, Info, "Loading complete for {}",
                   m_target == SceneState::Scene1 ? "Scene1" : "Scene2");
    }

    ~LoadingState() = default;

    static void describe(StateBuilder<LoadingState>& s) {
        s.opaque();
        s.system(&LoadingState::check_complete);
    }

    void check_complete(ResMut<GameFlow<SceneState>> flow) {
        if (m_loaded) {
            if (m_target == SceneState::Scene1) {
                flow->switch_to<Scene1State>();
            } else {
                flow->switch_to<Scene2State>();
            }
        }
    }

private:
    World* m_world = nullptr;
    SceneState m_target = SceneState::Scene1;
    AssetHandle m_mesh_handle{};
    bool m_loaded = false;
};

// ============================================================
// Scene1State -- DamagedHelmet
// ============================================================

class Scene1State : public State<SceneState> {
public:
    explicit Scene1State(World& world) : m_world(&world) {
        HELIOS_LOG(Scene, Info, "Scene1: Entering (DamagedHelmet + Physics)");

        auto& server = *world.resource<std::shared_ptr<AssetServer>>();

        // Find the loaded helmet mesh handle
        m_mesh_handle = server.load_sync<MeshAsset>(
            "Meshes/damaged_helmet_source_glb/scene.gltf");

        // Spawn helmet entity at Y=3 (physics will move it)
        m_helmet = spawn_tracked(world);
        world.add(m_helmet, Transform{
            .position = glm::vec3{0.0f, 3.0f, 0.0f},
            .rotation = glm::quat(glm::vec3(
                glm::radians(90.0f), glm::radians(180.0f), 0.0f))
        });
        world.add(m_helmet, MeshRenderer{
            .mesh = Handle<MeshAsset>::from(m_mesh_handle),
        });
        world.add(m_helmet, Tag{.name = "damaged_helmet"});

        HELIOS_LOG(Scene, Info, "Scene1: Spawned helmet entity");

        // --- Set up physics ---
        auto& demo = world.resource<PhysicsDemo>();
        if (!demo.physics) {
            HELIOS_LOG(Physics, Info, "Creating physics world");
            demo.physics = physics::create_physics_world();
        }

        // Static floor (large box at Y=-2)
        physics::BodyDesc floor_desc;
        floor_desc.type        = physics::BodyType::Static;
        floor_desc.position    = {0.0f, -2.0f, 0.0f};
        floor_desc.shape       = physics::BoxShape{{50.0f, 0.5f, 50.0f}};
        demo.floor_body = demo.physics->create_body(floor_desc, 1);

        // Dynamic helmet sphere at Y=3
        physics::BodyDesc helmet_desc;
        helmet_desc.type        = physics::BodyType::Dynamic;
        helmet_desc.position    = {0.0f, 3.0f, 0.0f};
        helmet_desc.shape       = physics::SphereShape{1.0f};
        helmet_desc.mass        = 2.0f;
        helmet_desc.restitution = 0.6f;
        demo.helmet_body = demo.physics->create_body(helmet_desc, 2);

        HELIOS_LOG(Physics, Info, "Floor body={} helmet body={}", demo.floor_body, demo.helmet_body);

        // --- Set up audio ---
        if (!demo.audio) {
            HELIOS_LOG(Audio, Info, "Creating audio device");
            demo.audio = audio::create_audio_device();
        }
        if (demo.bounce_wav.empty()) {
            demo.bounce_wav = generate_bounce_wav();
            HELIOS_LOG(Audio, Info, "Generated bounce WAV ({} bytes)", demo.bounce_wav.size());
        }

        demo.active = true;
        HELIOS_LOG(Scene, Info, "Scene1: Physics and audio ready. Press R to re-drop helmet.");
    }

    ~Scene1State() {
        HELIOS_LOG(Scene, Info, "Scene1: Exiting (DamagedHelmet)");
        // Clean up physics bodies (keep the world alive for re-entry)
        auto& demo = m_world->resource<PhysicsDemo>();
        if (demo.physics) {
            if (demo.helmet_body) demo.physics->destroy_body(demo.helmet_body);
            if (demo.floor_body)  demo.physics->destroy_body(demo.floor_body);
            demo.helmet_body = 0;
            demo.floor_body  = 0;
        }
        demo.active = false;

        // Release the mesh asset handle
        if (m_mesh_handle) {
            auto& server = *m_world->resource<std::shared_ptr<AssetServer>>();
            server.release(m_mesh_handle);
            auto unloaded = server.collect_garbage();
            if (!unloaded.empty()) {
                HELIOS_LOG(Scene, Info, "GC unloaded {} assets", unloaded.size());
            }
        }
    }

    static void describe(StateBuilder<Scene1State>& s) {
        s.opaque();
        s.system(&Scene1State::handle_input);
    }

    void handle_input(Res<RawInput> input,
                      ResMut<GameFlow<SceneState>> flow,
                      ResMut<PendingScene> pending) {
        if (input->key_just_pressed(KeyCode::Num2)) {
            HELIOS_LOG(Scene, Info, "Switching to Scene2 (Lion)...");
            pending->target = SceneState::Scene2;
            pending->pending = true;
            flow->switch_to<LoadingState>();
        }
    }

private:
    World* m_world = nullptr;
    Entity m_helmet{};
    AssetHandle m_mesh_handle{};
};

// ============================================================
// Scene2State -- Lion
// ============================================================

class Scene2State : public State<SceneState> {
public:
    explicit Scene2State(World& world) : m_world(&world) {
        HELIOS_LOG(Scene, Info, "Scene2: Entering (Lion)");

        auto& server = *world.resource<std::shared_ptr<AssetServer>>();

        // Find the loaded lion mesh handle
        m_mesh_handle = server.load_sync<MeshAsset>("Meshes/lion/scene.gltf");

        // Spawn lion entity -- rotated to face camera
        m_lion = spawn_tracked(world);
        world.add(m_lion, Transform{
            .position = glm::vec3{0.0f, -0.5f, 0.0f},
            .rotation = glm::quat(glm::vec3(
                glm::radians(0.0f), glm::radians(180.0f), 0.0f)),
            .scale = glm::vec3{0.01f}  // lion model is large, scale down
        });
        world.add(m_lion, MeshRenderer{
            .mesh = Handle<MeshAsset>::from(m_mesh_handle),
        });
        world.add(m_lion, Tag{.name = "lion"});

        HELIOS_LOG(Scene, Info, "Scene2: Spawned lion entity");
    }

    ~Scene2State() {
        HELIOS_LOG(Scene, Info, "Scene2: Exiting (Lion)");
        // Release the mesh asset handle
        if (m_mesh_handle) {
            auto& server = *m_world->resource<std::shared_ptr<AssetServer>>();
            server.release(m_mesh_handle);
            auto unloaded = server.collect_garbage();
            if (!unloaded.empty()) {
                HELIOS_LOG(Scene, Info, "GC unloaded {} assets", unloaded.size());
            }
        }
    }

    static void describe(StateBuilder<Scene2State>& s) {
        s.opaque();
        s.system(&Scene2State::handle_input);
    }

    void handle_input(Res<RawInput> input,
                      ResMut<GameFlow<SceneState>> flow,
                      ResMut<PendingScene> pending) {
        if (input->key_just_pressed(KeyCode::Num1)) {
            HELIOS_LOG(Scene, Info, "Switching to Scene1 (Helmet)...");
            pending->target = SceneState::Scene1;
            pending->pending = true;
            flow->switch_to<LoadingState>();
        }
    }

private:
    World* m_world = nullptr;
    Entity m_lion{};
    AssetHandle m_mesh_handle{};
};

// ============================================================
// Plugins
// ============================================================

struct GamePlugin {
    void build(App& app) {
        app.insert_resource(OrbitCamera{});
        app.insert_resource(PhysicsDemo{});
        app.add_system(Schedule::Update, orbit_camera_system, "orbit_camera");
        app.add_system(Schedule::Update, physics_update_system, "physics_update");
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

        HELIOS_LOG(Scene, Info, "Scene: camera + sun spawned (no mesh yet -- states handle that)");
    }
};

// ============================================================
// SandboxAssetsPlugin: sets up shared GPU infrastructure
// (PBR pipeline, skybox, depth buffer -- scene-independent)
// Per-mesh/per-material data is now handled by GPUResourceCache.
// ============================================================

struct SandboxAssetsPlugin {
    void build(App& app) {
        auto& ctx = app.world().resource<RenderContext>();
        auto& device = *ctx.device;

        const std::string asset_dir = HELIOS_DEMO_ASSET_DIR;

#ifdef HELIOS_SHADER_DIR
        const std::string shader_dir = HELIOS_SHADER_DIR;
#else
        const std::string shader_dir = "shaders";
#endif

        PBRRenderState pbr;
        SkyboxState skybox;

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
            ctx.depth_texture = device.create_texture(depth_desc);
        }

        // ---- PBR Camera UBO (set 0, binding 0) ----
        {
            rhi::BufferDesc ubo_desc;
            ubo_desc.size = sizeof(PBRCameraUBO);
            ubo_desc.usage = rhi::BufferUsage::Uniform;
            ubo_desc.access = rhi::MemoryAccess::CPU_to_GPU;
            ubo_desc.debug_name = "PBRCameraUBO";
            pbr.camera_ubo = device.create_buffer(ubo_desc);
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
            pbr.camera_layout = device.create_descriptor_set_layout(layout_desc);
        }

        // ---- Material descriptor set layout (set 1) ----
        {
            rhi::DescriptorSetLayoutDesc layout_desc;
            layout_desc.bindings = {
                rhi::DescriptorBinding{
                    .binding = 0,
                    .type = rhi::DescriptorType::CombinedImageSampler,
                    .stage = rhi::ShaderStage::Fragment, .count = 1,
                },
                rhi::DescriptorBinding{
                    .binding = 1,
                    .type = rhi::DescriptorType::CombinedImageSampler,
                    .stage = rhi::ShaderStage::Fragment, .count = 1,
                },
                rhi::DescriptorBinding{
                    .binding = 2,
                    .type = rhi::DescriptorType::CombinedImageSampler,
                    .stage = rhi::ShaderStage::Fragment, .count = 1,
                },
                rhi::DescriptorBinding{
                    .binding = 3,
                    .type = rhi::DescriptorType::CombinedImageSampler,
                    .stage = rhi::ShaderStage::Fragment, .count = 1,
                },
                rhi::DescriptorBinding{
                    .binding = 4,
                    .type = rhi::DescriptorType::CombinedImageSampler,
                    .stage = rhi::ShaderStage::Fragment, .count = 1,
                },
            };
            layout_desc.debug_name = "PBRMaterial_DSL";
            pbr.material_layout = device.create_descriptor_set_layout(layout_desc);
        }

        const rhi::TextureFormat swapchain_color_fmt = ctx.swapchain->color_format();

        // ---- Load PBR shaders ----
        {
            auto vert_spirv = sandbox::read_spirv(std::filesystem::path(shader_dir) / "pbr_simple.vert.spv");
            auto frag_spirv = sandbox::read_spirv(std::filesystem::path(shader_dir) / "pbr_simple.frag.spv");
            if (vert_spirv.empty() || frag_spirv.empty()) {
                HELIOS_LOG(Game, Error, "Failed to load PBR shaders from '{}'", shader_dir);
                app.insert_resource(std::move(pbr));
                app.insert_resource(std::move(skybox));
                return;
            }

            rhi::ShaderDesc vert_desc;
            vert_desc.stage = rhi::ShaderStage::Vertex;
            vert_desc.spirv_code = std::move(vert_spirv);
            vert_desc.entry_point = "main";
            vert_desc.debug_name = "pbr_simple_vert";
            pbr.vert_shader = device.create_shader(vert_desc);

            rhi::ShaderDesc frag_desc;
            frag_desc.stage = rhi::ShaderStage::Fragment;
            frag_desc.spirv_code = std::move(frag_spirv);
            frag_desc.entry_point = "main";
            frag_desc.debug_name = "pbr_simple_frag";
            pbr.frag_shader = device.create_shader(frag_desc);
        }

        // ---- PBR graphics pipeline ----
        // Note: vertex layout matches PBRVertex from mesh_asset.h
        {
            rhi::GraphicsPipelineDesc pipe_desc;
            pipe_desc.vertex_shader = pbr.vert_shader.get();
            pipe_desc.fragment_shader = pbr.frag_shader.get();
            pipe_desc.layout.stride = sizeof(PBRVertex);
            pipe_desc.layout.attributes = {
                rhi::VertexAttribute{
                    .location = 0, .binding = 0, .offset = 0,
                    .format = rhi::TextureFormat::RGB32F,
                },
                rhi::VertexAttribute{
                    .location = 1, .binding = 0,
                    .offset = sizeof(glm::vec3),
                    .format = rhi::TextureFormat::RGB32F,
                },
                rhi::VertexAttribute{
                    .location = 2, .binding = 0,
                    .offset = sizeof(glm::vec3) * 2,
                    .format = rhi::TextureFormat::RG32F,
                },
                rhi::VertexAttribute{
                    .location = 3, .binding = 0,
                    .offset = sizeof(glm::vec3) * 2 + sizeof(glm::vec2),
                    .format = rhi::TextureFormat::RGBA32F,
                },
            };
            pipe_desc.state.cull = rhi::CullMode::Back;
            pipe_desc.state.depth = rhi::DepthCompare::Less;
            pipe_desc.state.depth_test = true;
            pipe_desc.state.depth_write = true;
            pipe_desc.state.blend = rhi::BlendMode::None;
            pipe_desc.render_pass = nullptr;
            pipe_desc.descriptor_layouts = {
                pbr.camera_layout.get(),
                pbr.material_layout.get()
            };
            pipe_desc.push_constant_size = sizeof(PushConstantData);
            pipe_desc.push_constant_stages = rhi::ShaderStage::Vertex;
            pipe_desc.debug_name = "PBRSimple";
            pipe_desc.use_dynamic_rendering = true;
            pipe_desc.dynamic_color_formats = { swapchain_color_fmt };
            pipe_desc.dynamic_depth_format = rhi::TextureFormat::Depth32F;

            pbr.pipeline = device.create_graphics_pipeline(pipe_desc);
            if (!pbr.pipeline) {
                HELIOS_LOG(Game, Error, "Failed to create PBR pipeline");
                app.insert_resource(std::move(pbr));
                app.insert_resource(std::move(skybox));
                return;
            }
        }

        // ---- Camera descriptor set ----
        {
            pbr.camera_ds = device.allocate_descriptor_set(*pbr.camera_layout);
            device.update_descriptor_set(*pbr.camera_ds, {
                rhi::DescriptorWrite{
                    .binding = 0,
                    .type = rhi::DescriptorType::UniformBuffer,
                    .buffer_handle = pbr.camera_ubo.get(),
                    .range = sizeof(PBRCameraUBO),
                },
            });
        }

        pbr.valid = true;
        HELIOS_LOG(Game, Info, "PBR pipeline created");

        // ============================================================
        // Skybox setup (shared between all scenes)
        // ============================================================
        std::unique_ptr<rhi::Texture> env_cubemap;
        {
            auto equirect = sandbox::load_hdr_texture(device,
                (asset_dir + "/Textures/default_skybox.hdr").c_str(), "SkyboxEquirect");
            if (equirect) {
                env_cubemap = sandbox::convert_equirect_to_cubemap(
                    device, *ctx.cmd, *equirect, 1024);
            }
        }

        if (env_cubemap) {
            auto sky_vert_spirv = sandbox::read_spirv(std::filesystem::path(shader_dir) / "skybox.vert.spv");
            auto sky_frag_spirv = sandbox::read_spirv(std::filesystem::path(shader_dir) / "skybox.frag.spv");

            if (!sky_vert_spirv.empty() && !sky_frag_spirv.empty()) {
                rhi::ShaderDesc sv_desc;
                sv_desc.stage = rhi::ShaderStage::Vertex;
                sv_desc.spirv_code = std::move(sky_vert_spirv);
                sv_desc.entry_point = "main";
                sv_desc.debug_name = "skybox_vert";
                skybox.vert_shader = device.create_shader(sv_desc);

                rhi::ShaderDesc sf_desc;
                sf_desc.stage = rhi::ShaderStage::Fragment;
                sf_desc.spirv_code = std::move(sky_frag_spirv);
                sf_desc.entry_point = "main";
                sf_desc.debug_name = "skybox_frag";
                skybox.frag_shader = device.create_shader(sf_desc);

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
                skybox.layout = device.create_descriptor_set_layout(sky_layout_desc);

                rhi::BufferDesc sky_ubo_desc;
                sky_ubo_desc.size = sizeof(SkyboxUBOData);
                sky_ubo_desc.usage = rhi::BufferUsage::Uniform;
                sky_ubo_desc.access = rhi::MemoryAccess::CPU_to_GPU;
                sky_ubo_desc.debug_name = "SkyboxUBO";
                skybox.ubo = device.create_buffer(sky_ubo_desc);

                rhi::GraphicsPipelineDesc sky_pipe;
                sky_pipe.vertex_shader = skybox.vert_shader.get();
                sky_pipe.fragment_shader = skybox.frag_shader.get();
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
                sky_pipe.descriptor_layouts = { skybox.layout.get() };
                sky_pipe.push_constant_size = 0;
                sky_pipe.debug_name = "SkyboxPipeline";
                sky_pipe.use_dynamic_rendering = true;
                sky_pipe.dynamic_color_formats = { swapchain_color_fmt };
                sky_pipe.dynamic_depth_format = rhi::TextureFormat::Depth32F;

                skybox.pipeline = device.create_graphics_pipeline(sky_pipe);

                auto sky_verts = sandbox::build_skybox_cube();
                rhi::BufferDesc sky_vbo_desc;
                sky_vbo_desc.size = static_cast<uint32_t>(sky_verts.size() * sizeof(glm::vec3));
                sky_vbo_desc.usage = rhi::BufferUsage::Vertex;
                sky_vbo_desc.access = rhi::MemoryAccess::CPU_to_GPU;
                sky_vbo_desc.debug_name = "SkyboxCubeVBO";
                skybox.cube_vbo = device.create_buffer(sky_vbo_desc, sky_verts.data());

                skybox.env_cubemap = std::move(env_cubemap);

                skybox.ds = device.allocate_descriptor_set(*skybox.layout);
                device.update_descriptor_set(*skybox.ds, {
                    rhi::DescriptorWrite{
                        .binding = 0,
                        .type = rhi::DescriptorType::UniformBuffer,
                        .buffer_handle = skybox.ubo.get(),
                        .range = sizeof(SkyboxUBOData),
                    },
                    rhi::DescriptorWrite{
                        .binding = 1,
                        .type = rhi::DescriptorType::CombinedImageSampler,
                        .texture_handle = skybox.env_cubemap.get(),
                    },
                });

                skybox.valid = true;
                HELIOS_LOG(Game, Info, "Skybox pipeline created");
            }
        }

        app.insert_resource(std::move(pbr));
        app.insert_resource(std::move(skybox));
        HELIOS_LOG(Game, Info, "SandboxAssetsPlugin: shared GPU resources ready");
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

    HELIOS_LOG(Core, Info, "=== Helios PBR Sandbox (Asset Pipeline Demo) ===");

    App app;

    // Engine plugins (order matters: Window -> Render -> ForwardPlus)
    app.add_plugin(WindowPlugin{ .primary_window = WindowDesc{
        .title = "Helios PBR Sandbox",
        .width = 1280,
        .height = 720,
    }});
    app.add_plugin(InputPlugin{});
    app.add_plugin(RenderPlugin{});

    // Asset pipeline -- registers AssetServer + MeshAsset importer
    app.add_plugin(AssetPlugin{AssetPluginConfig{
        .asset_root = HELIOS_DEMO_ASSET_DIR,
        .loader_threads = 0,  // sync-only for this demo
    }});

    app.add_plugin(ForwardPlusPlugin{});

    app.insert_resource(PendingScene{.target = SceneState::Scene1, .pending = true});

    // Shared GPU resources (pipeline, skybox)
    app.add_plugin(SandboxAssetsPlugin{});

    // Game plugins (camera, etc.)
    app.add_plugin(GamePlugin{});
    app.add_plugin(ScenePlugin{});

    // Register despawn hook: release MeshRenderer asset handles automatically
    {
        auto& world = app.world();
        world.register_despawn_hook([](World& w, Entity e) {
            auto* mr = w.try_get<MeshRenderer>(e);
            if (!mr) return;
            auto* server_ptr = w.try_resource<std::shared_ptr<AssetServer>>();
            if (!server_ptr || !*server_ptr) return;
            auto& server = **server_ptr;
            if (mr->mesh) server.release(mr->mesh.untyped());
            if (mr->material) server.release(mr->material.untyped());
        });
    }

    // State management
    app.add_plugin(GameFlowPlugin<SceneState>{}
        .state<LoadingState>(SceneState::Loading)
        .state<Scene1State>(SceneState::Scene1)
        .state<Scene2State>(SceneState::Scene2)
        .initial<LoadingState>()
    );

    HELIOS_LOG(Core, Info, "All plugins loaded. Starting engine...");
    HELIOS_LOG(Game, Info, "Controls: 1 = Scene1 (Helmet+Physics), 2 = Scene2 (Lion), R = re-drop helmet, RMB = orbit camera");

    app.run();

    HELIOS_LOG(Core, Info, "=== Sandbox shutdown ===");
    return 0;
}
