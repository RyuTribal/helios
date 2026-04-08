// Helios Engine - PBR Sandbox Demo (3-state version)
//
// Demonstrates asset loading/unloading between scenes using the GameFlow
// state machine. Three states:
//   Loading  -- async-loads assets for the next scene
//   Scene1   -- DamagedHelmet + skybox
//   Scene2   -- Lion + skybox (shared skybox, helmet textures get GC'd)
//
// Press 1 to switch to Scene1, 2 to switch to Scene2.

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
#include <helios/forward_plus/pbr_render_state.h>
#include <helios/forward_plus/skybox_state.h>
#include <helios/forward_plus/gpu_data.h>
#include <helios/graph/frame_packet.h>
#include <helios/app/game_flow_plugin.h>
#include <helios/app/state.h>
#include <helios/app/state_builder.h>
#include <helios/assets/asset_server.h>

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
// State enum
// ============================================================

enum class SceneState { Loading, Scene1, Scene2 };

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
// Scene asset handle tracking (resource shared across states)
// ============================================================

struct SceneAssets {
    // Per-scene GPU resources (owned by the active scene state)
    std::unique_ptr<rhi::Buffer> mesh_vbo;
    std::unique_ptr<rhi::Buffer> mesh_ibo;
    uint32_t index_count = 0;

    std::unique_ptr<rhi::Texture> albedo_tex;
    std::unique_ptr<rhi::Texture> normal_tex;
    std::unique_ptr<rhi::Texture> metallic_roughness_tex;
    std::unique_ptr<rhi::Texture> emissive_tex;

    // Asset handles for refcount tracking
    std::vector<AssetHandle> tracked_handles;

    void clear() {
        mesh_vbo.reset();
        mesh_ibo.reset();
        index_count = 0;
        albedo_tex.reset();
        normal_tex.reset();
        metallic_roughness_tex.reset();
        emissive_tex.reset();
        tracked_handles.clear();
    }
};

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
// Helper: update material descriptor set with current scene textures
// ============================================================

static void update_material_ds(rhi::Device& device,
                                PBRRenderState& pbr,
                                const SceneAssets& scene,
                                const SkyboxState& skybox) {
    if (!pbr.material_ds) return;

    // Use env cubemap for binding 4 if available, else albedo as fallback
    rhi::Texture* env_tex = skybox.env_cubemap ? skybox.env_cubemap.get()
                                               : scene.albedo_tex.get();

    device.update_descriptor_set(*pbr.material_ds, {
        rhi::DescriptorWrite{
            .binding = 0,
            .type = rhi::DescriptorType::CombinedImageSampler,
            .texture_handle = scene.albedo_tex.get(),
        },
        rhi::DescriptorWrite{
            .binding = 1,
            .type = rhi::DescriptorType::CombinedImageSampler,
            .texture_handle = scene.normal_tex.get(),
        },
        rhi::DescriptorWrite{
            .binding = 2,
            .type = rhi::DescriptorType::CombinedImageSampler,
            .texture_handle = scene.metallic_roughness_tex.get(),
        },
        rhi::DescriptorWrite{
            .binding = 3,
            .type = rhi::DescriptorType::CombinedImageSampler,
            .texture_handle = scene.emissive_tex.get(),
        },
        rhi::DescriptorWrite{
            .binding = 4,
            .type = rhi::DescriptorType::CombinedImageSampler,
            .texture_handle = env_tex,
        },
    });
}

// ============================================================
// Helper: load helmet scene assets
// ============================================================

static SceneAssets load_helmet_assets(rhi::Device& device) {
    SceneAssets scene;

    const std::string asset_dir = HELIOS_DEMO_ASSET_DIR;
    const std::string helmet_dir = asset_dir + "/Meshes/damaged_helmet_source_glb";
    const std::string tex_dir = helmet_dir + "/textures";

    // Load mesh
    auto mesh = sandbox::load_gltf_mesh(device, (helmet_dir + "/scene.gltf").c_str());
    scene.mesh_vbo = std::move(mesh.vbo);
    scene.mesh_ibo = std::move(mesh.ibo);
    scene.index_count = mesh.index_count;

    // Load textures
    scene.albedo_tex = sandbox::load_texture_2d(device,
        (tex_dir + "/Material_MR_baseColor.jpeg").c_str(), "HelmetAlbedo");
    scene.normal_tex = sandbox::load_texture_2d(device,
        (tex_dir + "/Material_MR_normal.jpeg").c_str(), "HelmetNormal");
    scene.metallic_roughness_tex = sandbox::load_texture_2d(device,
        (tex_dir + "/Material_MR_metallicRoughness.png").c_str(), "HelmetMR");
    scene.emissive_tex = sandbox::load_texture_2d(device,
        (tex_dir + "/Material_MR_emissive.jpeg").c_str(), "HelmetEmissive");

    return scene;
}

// ============================================================
// Helper: load lion scene assets
// ============================================================

static SceneAssets load_lion_assets(rhi::Device& device) {
    SceneAssets scene;

    const std::string asset_dir = HELIOS_DEMO_ASSET_DIR;
    const std::string lion_dir = asset_dir + "/Meshes/lion";
    const std::string tex_dir = lion_dir + "/textures";

    // Load mesh
    auto mesh = sandbox::load_gltf_mesh(device, (lion_dir + "/scene.gltf").c_str());
    scene.mesh_vbo = std::move(mesh.vbo);
    scene.mesh_ibo = std::move(mesh.ibo);
    scene.index_count = mesh.index_count;

    // Load textures -- lion has baseColor and normal only
    scene.albedo_tex = sandbox::load_texture_2d(device,
        (tex_dir + "/material0000_baseColor.png").c_str(), "LionAlbedo");
    scene.normal_tex = sandbox::load_texture_2d(device,
        (tex_dir + "/material0000_normal.jpeg").c_str(), "LionNormal");

    // Create a 1x1 white texture for metallic-roughness (default)
    {
        uint8_t white_pixel[4] = {255, 255, 255, 255};
        rhi::TextureDesc desc;
        desc.width = 1; desc.height = 1;
        desc.format = rhi::TextureFormat::RGBA8;
        desc.type = rhi::TextureType::Texture2D;
        desc.mip_levels = 1; desc.array_layers = 1;
        desc.usage = rhi::TextureUsage::Sampled;
        desc.debug_name = "LionMR_Default";
        scene.metallic_roughness_tex = device.create_texture(desc, white_pixel);
    }

    // Create a 1x1 black texture for emissive (default)
    {
        uint8_t black_pixel[4] = {0, 0, 0, 255};
        rhi::TextureDesc desc;
        desc.width = 1; desc.height = 1;
        desc.format = rhi::TextureFormat::RGBA8;
        desc.type = rhi::TextureType::Texture2D;
        desc.mip_levels = 1; desc.array_layers = 1;
        desc.usage = rhi::TextureUsage::Sampled;
        desc.debug_name = "LionEmissive_Default";
        scene.emissive_tex = device.create_texture(desc, black_pixel);
    }

    return scene;
}

// ============================================================
// AssetServer for tracking -- lightweight wrapper for refcounts
// ============================================================

/// Simplified asset tracking resource. Wraps an AssetServer for refcount/GC
/// and also holds per-scene handles.
struct AssetTracker {
    std::unique_ptr<AssetServer> server;

    // Skybox handle -- shared between scenes, acquired by both
    AssetHandle skybox_handle{};

    AssetTracker() = default;

    explicit AssetTracker(const std::filesystem::path& root)
        : server(std::make_unique<AssetServer>(root, 0))  // 0 loader threads (sync only)
    {
        // Register a trivial importer for tracking purposes
        server->register_importer<std::string>(
            [](const std::filesystem::path& path) -> std::any {
                return std::any(path.string());
            });
    }

    // Create tracked handles for a scene's assets and acquire them
    std::vector<AssetHandle> track_scene(const std::string& prefix, int count) {
        std::vector<AssetHandle> handles;
        for (int i = 0; i < count; ++i) {
            std::string path = prefix + "/asset_" + std::to_string(i);
            auto h = server->load_sync<std::string>(path);
            if (h) {
                server->acquire(h);
                handles.push_back(h);
            }
        }
        return handles;
    }

    // Release all handles for a scene
    void release_scene(const std::vector<AssetHandle>& handles) {
        for (auto h : handles) {
            server->release(h);
        }
    }

    // Run GC and log unloaded assets
    std::vector<AssetHandle> gc() {
        return server->collect_garbage();
    }
};

// ============================================================
// Forward declarations of states
// ============================================================

class LoadingState;
class Scene1State;
class Scene2State;

// ============================================================
// LoadingState
// ============================================================

class LoadingState : public State<SceneState> {
public:
    explicit LoadingState(World& world) : m_world(&world) {
        auto& pending = world.resource<PendingScene>();
        m_target = pending.target;
        pending.pending = false;

        HELIOS_LOG(Scene, Info, "Loading assets for {}...",
                   m_target == SceneState::Scene1 ? "Scene1 (Helmet)" : "Scene2 (Lion)");

        // Load synchronously (in a real engine this would be async)
        auto& ctx = world.resource<RenderContext>();
        auto& device = *ctx.device;

        auto& scene_assets = world.resource<SceneAssets>();
        auto& tracker = world.resource<AssetTracker>();

        // Release old scene handles and run GC
        tracker.release_scene(scene_assets.tracked_handles);
        auto unloaded = tracker.gc();
        if (!unloaded.empty()) {
            HELIOS_LOG(Scene, Info, "GC unloaded {} assets during transition", unloaded.size());
        }

        // Clear old GPU resources
        device.wait_idle();
        scene_assets.clear();

        // Load new scene
        if (m_target == SceneState::Scene1) {
            scene_assets = load_helmet_assets(device);
            scene_assets.tracked_handles = tracker.track_scene("helmet", 5);
        } else {
            scene_assets = load_lion_assets(device);
            scene_assets.tracked_handles = tracker.track_scene("lion", 4);
        }

        // Update PBR render state with new mesh/textures
        auto& pbr = world.resource<PBRRenderState>();
        auto& skybox = world.resource<SkyboxState>();

        if (scene_assets.mesh_vbo && scene_assets.mesh_ibo &&
            scene_assets.albedo_tex && scene_assets.normal_tex &&
            scene_assets.metallic_roughness_tex && scene_assets.emissive_tex) {

            // Move mesh data into PBR state
            pbr.mesh_vbo = std::move(scene_assets.mesh_vbo);
            pbr.mesh_ibo = std::move(scene_assets.mesh_ibo);
            pbr.index_count = scene_assets.index_count;

            // Move textures into PBR state
            pbr.albedo_tex = std::move(scene_assets.albedo_tex);
            pbr.normal_tex = std::move(scene_assets.normal_tex);
            pbr.metallic_roughness_tex = std::move(scene_assets.metallic_roughness_tex);
            pbr.emissive_tex = std::move(scene_assets.emissive_tex);

            // Update the material descriptor set with the new textures
            rhi::Texture* env_tex = skybox.env_cubemap ? skybox.env_cubemap.get()
                                                       : pbr.albedo_tex.get();
            device.update_descriptor_set(*pbr.material_ds, {
                rhi::DescriptorWrite{
                    .binding = 0,
                    .type = rhi::DescriptorType::CombinedImageSampler,
                    .texture_handle = pbr.albedo_tex.get(),
                },
                rhi::DescriptorWrite{
                    .binding = 1,
                    .type = rhi::DescriptorType::CombinedImageSampler,
                    .texture_handle = pbr.normal_tex.get(),
                },
                rhi::DescriptorWrite{
                    .binding = 2,
                    .type = rhi::DescriptorType::CombinedImageSampler,
                    .texture_handle = pbr.metallic_roughness_tex.get(),
                },
                rhi::DescriptorWrite{
                    .binding = 3,
                    .type = rhi::DescriptorType::CombinedImageSampler,
                    .texture_handle = pbr.emissive_tex.get(),
                },
                rhi::DescriptorWrite{
                    .binding = 4,
                    .type = rhi::DescriptorType::CombinedImageSampler,
                    .texture_handle = env_tex,
                },
            });

            pbr.valid = true;
        } else {
            HELIOS_LOG(Scene, Error, "Failed to load scene assets");
            pbr.valid = false;
        }

        m_loaded = true;
        HELIOS_LOG(Scene, Info, "Loading complete for {}",
                   m_target == SceneState::Scene1 ? "Scene1" : "Scene2");
    }

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
    bool m_loaded = false;
};

// ============================================================
// Scene1State -- DamagedHelmet
// ============================================================

class Scene1State : public State<SceneState> {
public:
    explicit Scene1State(World& world) : m_world(&world) {
        HELIOS_LOG(Scene, Info, "Scene1: Entering (DamagedHelmet)");

        // Spawn helmet entity
        m_helmet = spawn_tracked(world);
        world.add(m_helmet, Transform{
            .position = glm::vec3{0.0f, 0.0f, 0.0f},
            .rotation = glm::quat(glm::vec3(
                glm::radians(90.0f), glm::radians(180.0f), 0.0f))
        });
        world.add(m_helmet, MeshRenderer{
            .mesh = AssetHandle{1, 1},
            .material = AssetHandle{1, 1}
        });
        world.add(m_helmet, Tag{.name = "damaged_helmet"});

        HELIOS_LOG(Scene, Info, "Scene1: Spawned helmet entity");
    }

    ~Scene1State() {
        HELIOS_LOG(Scene, Info, "Scene1: Exiting (DamagedHelmet)");
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
};

// ============================================================
// Scene2State -- Lion
// ============================================================

class Scene2State : public State<SceneState> {
public:
    explicit Scene2State(World& world) : m_world(&world) {
        HELIOS_LOG(Scene, Info, "Scene2: Entering (Lion)");

        // Spawn lion entity -- rotated to face camera
        m_lion = spawn_tracked(world);
        world.add(m_lion, Transform{
            .position = glm::vec3{0.0f, -0.5f, 0.0f},
            .rotation = glm::quat(glm::vec3(
                glm::radians(0.0f), glm::radians(180.0f), 0.0f)),
            .scale = glm::vec3{0.01f}  // lion model is large, scale down
        });
        world.add(m_lion, MeshRenderer{
            .mesh = AssetHandle{1, 1},
            .material = AssetHandle{1, 1}
        });
        world.add(m_lion, Tag{.name = "lion"});

        HELIOS_LOG(Scene, Info, "Scene2: Spawned lion entity");
    }

    ~Scene2State() {
        HELIOS_LOG(Scene, Info, "Scene2: Exiting (Lion)");
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
};

// ============================================================
// Plugins
// ============================================================

struct GamePlugin {
    void build(App& app) {
        app.insert_resource(OrbitCamera{});
        app.add_system(Schedule::Update, orbit_camera_system, "orbit_camera");
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

        HELIOS_LOG(Scene, Info, "Scene: camera + sun spawned (no mesh yet -- states handle that)");
    }
};

// ============================================================
// SandboxAssetsPlugin: sets up shared GPU infrastructure
// (PBR pipeline, skybox, depth buffer -- scene-independent)
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
            auto vert_spirv = read_spirv(std::filesystem::path(shader_dir) / "pbr_simple.vert.spv");
            auto frag_spirv = read_spirv(std::filesystem::path(shader_dir) / "pbr_simple.frag.spv");
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
        {
            rhi::GraphicsPipelineDesc pipe_desc;
            pipe_desc.vertex_shader = pbr.vert_shader.get();
            pipe_desc.fragment_shader = pbr.frag_shader.get();
            pipe_desc.layout.stride = sizeof(sandbox::PBRVertex);
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

        // ---- Material descriptor set (initially empty, filled by scene states) ----
        {
            pbr.material_ds = device.allocate_descriptor_set(*pbr.material_layout);
        }

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
            auto sky_vert_spirv = read_spirv(std::filesystem::path(shader_dir) / "skybox.vert.spv");
            auto sky_frag_spirv = read_spirv(std::filesystem::path(shader_dir) / "skybox.frag.spv");

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

                auto sky_verts = build_skybox_cube();
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

    HELIOS_LOG(Core, Info, "=== Helios PBR Sandbox (3-State Demo) ===");

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

    // Asset tracking resource
    app.insert_resource(AssetTracker{HELIOS_DEMO_ASSET_DIR});
    app.insert_resource(SceneAssets{});
    app.insert_resource(PendingScene{.target = SceneState::Scene1, .pending = true});
    app.insert_resource(renderer::FramePacket{});

    // Shared GPU resources (pipeline, skybox)
    app.add_plugin(SandboxAssetsPlugin{});

    // Game plugins (camera, etc.)
    app.add_plugin(GamePlugin{});
    app.add_plugin(ScenePlugin{});

    // Acquire skybox handle in the tracker (shared, never GC'd)
    {
        auto& tracker = app.world().resource<AssetTracker>();
        auto skybox_h = tracker.server->load_sync<std::string>("shared/skybox");
        if (skybox_h) {
            tracker.server->acquire(skybox_h);
            tracker.server->acquire(skybox_h);  // 2 refs: one per scene
            tracker.skybox_handle = skybox_h;
        }
    }

    // State management
    app.add_plugin(GameFlowPlugin<SceneState>{}
        .state<LoadingState>(SceneState::Loading)
        .state<Scene1State>(SceneState::Scene1)
        .state<Scene2State>(SceneState::Scene2)
        .initial<LoadingState>()
    );

    HELIOS_LOG(Core, Info, "All plugins loaded. Starting engine...");
    HELIOS_LOG(Game, Info, "Controls: Press 1 = Scene1 (Helmet), 2 = Scene2 (Lion), RMB = orbit camera");

    app.run();

    HELIOS_LOG(Core, Info, "=== Sandbox shutdown ===");
    return 0;
}
