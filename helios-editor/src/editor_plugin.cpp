#include "editor_plugin.h"
#include "editor_state.h"
#include "editor_commands.h"
#include "editor_camera.h"
#include "editor_resources.h"
#include "project/project.h"
#include "imgui/imgui_render_plugin.h"
#include "panels/scene_hierarchy.h"
#include "panels/inspector.h"
#include "panels/content_browser.h"
#include "panels/viewport.h"
#include "panels/project_settings.h"
#include "panels/scene_settings.h"
#include "panels/console.h"


#include <helios/ecs/world.h>
#include <helios/ecs/time.h>
#include <helios/render_plugin.h>
#include <helios/assets/asset_utils.h>
#include <interface/physics_factory.h>
#include <helios/script/scripting_plugin.h>
#include <helios/script/script_instance.h>
#include <helios/script/script_runtime.h>
#include <helios/render_settings.h>
#include <helios/graph/frame_packet.h>
#include <helios/serialization/scene_serializer.h>
#include <helios/serialization/yaml_serializer.h>
#include <helios/core/log_macros.h>
#include <helios/core/logging.h>
#include <helios/window/windows.h>

#include <algorithm>
#include <imgui.h>
#include <imgui_internal.h>
#include <nfd.h>
#include "icons/IconsMaterialDesignIcons.h"

HELIOS_DEFINE_LOG_CHANNEL(Editor);

HELIOS_DECLARE_LOG_CHANNEL(Render);

namespace helios::editor {

// Toggle EditorCamera's ActiveCamera on Play/Stop.
static void set_editor_camera_active(World& world, bool active) {
    auto q = world.query<const Tag, const Camera>();
    for (auto [e, tag, cam] : q.with_entity()) {
        if (!world.has<EditorOnly>(e)) continue;
        if (active && !world.has<ActiveCamera>(e))
            world.add(e, ActiveCamera{});
        else if (!active && world.has<ActiveCamera>(e))
            world.remove<ActiveCamera>(e);
        break;
    }
}

// Enter Play: snapshot scene, create physics world, swap camera.
// Helper: despawn all game entities (everything except EditorCamera)
static void despawn_all_game_entities(World& world) {
    std::vector<Entity> to_despawn;
    auto q = world.query<const Tag, Without<Parent>>();
    for (auto [e, tag] : q.with_entity()) {
        if (world.has<EditorOnly>(e)) continue;
        to_despawn.push_back(e);
    }
    for (auto e : to_despawn) {
        if (world.is_alive(e)) world.despawn(e);
    }
}


static void enter_play_mode(World& world, EditorState& state) {
    state.mode = EditorMode::Play;

    // Snapshot the edit state so Stop can restore it
    if (auto* ser = world.try_resource<SceneSerializer>()) {
        state.scene_snapshot = ser->save_to_string(world);
    }

    // Despawn the edit scene
    despawn_all_game_entities(world);

    // Load the project's starting scene from disk
    auto* project = world.try_resource<Project>();
    if (project && !project->project_dir.empty()) {
        auto starting = project->scene_dir() / "Main_Scene.hvescn";
        if (auto* ser = world.try_resource<SceneSerializer>()) {
            if (std::filesystem::exists(starting)) {
                Entity root = ser->load_scene(world, starting);
                if (auto* edit = world.try_resource<SceneEditState>()) {
                    edit->active_scene = root;
                }
            }
        }
        resolve_mesh_paths(world);
    }

    // Create physics world
    if (auto* pw = world.try_resource<std::unique_ptr<physics::PhysicsWorld>>()) {
        auto* cfg = world.try_resource<physics::PhysicsConfig>();
        *pw = physics::create_physics_world(cfg ? *cfg : physics::PhysicsConfig{});
    }

    // Enable script execution
    if (auto* exec = world.try_resource<ScriptExecutionState>()) {
        exec->running = true;
    }

    set_editor_camera_active(world, false);
    HELIOS_LOG(Editor, Info, "Entered Play mode");
}

// Exit Play: destroy physics, restore edit state from snapshot, swap camera.
static void exit_play_mode(World& world, EditorState& state) {
    state.mode = EditorMode::Edit;

    // Destroy physics world
    if (auto* pw = world.try_resource<std::unique_ptr<physics::PhysicsWorld>>()) {
        pw->reset();
    }
    if (auto* bm = world.try_resource<physics::PhysicsBodyMap>()) {
        bm->entries.clear();
    }

    // Disable script execution
    if (auto* exec = world.try_resource<ScriptExecutionState>()) {
        exec->running = false;
    }

    // Destroy all managed script instances before despawning entities
    if (auto* rt_ptr = world.try_resource<std::unique_ptr<ScriptRuntime>>()) {
        if (*rt_ptr) (*rt_ptr)->destroy_all_instances();
    }

    // Despawn play scene, restore edit state
    despawn_all_game_entities(world);

    if (!state.scene_snapshot.empty()) {
        if (auto* ser = world.try_resource<SceneSerializer>()) {
            Entity root = ser->load_scene_from_string(world, state.scene_snapshot);
            if (auto* edit = world.try_resource<SceneEditState>()) {
                edit->active_scene = root;
            }
        }
        state.scene_snapshot.clear();
    }

    resolve_mesh_paths(world);
    set_editor_camera_active(world, true);
    HELIOS_LOG(Editor, Info, "Returned to Edit mode");
}

// Bridge ECS collider components to physics::Collider.
// The inspector adds BoxCollider/SphereCollider (serializable ECS components).
// The physics system queries physics::Collider. This sync creates the bridge.
static void sync_colliders_to_physics(World& world) {
    // BoxCollider → physics::Collider
    auto box_q = world.query<const BoxCollider, Without<physics::Collider>>();
    for (auto [e, bc] : box_q.with_entity()) {
        world.add(e, physics::Collider{
            .shape = physics::BoxShape{bc.half_extents}
        });
    }

    // SphereCollider → physics::Collider
    auto sphere_q = world.query<const SphereCollider, Without<physics::Collider>>();
    for (auto [e, sc] : sphere_q.with_entity()) {
        world.add(e, physics::Collider{
            .shape = physics::SphereShape{sc.radius}
        });
    }
}

// Save project file and all scenes with a SceneRoot component.
static void save_all(World& world) {
    auto* project = world.try_resource<Project>();
    if (project) project->save();

    auto* ser = world.try_resource<SceneSerializer>();
    auto* edit_state = world.try_resource<SceneEditState>();
    if (ser && project) {
        auto scene_query = world.query<const SceneRoot>();
        for (auto [entity, sr] : scene_query.with_entity()) {
            if (!sr.scene_path.empty()) {
                auto full_path = project->asset_dir() / sr.scene_path;
                std::filesystem::create_directories(full_path.parent_path());
                ser->save_entities(world, {entity}, full_path);
                if (edit_state) edit_state->clear_dirty(entity);
                HELIOS_LOG(Editor, Info, "Saved scene: {}", sr.scene_path);
            }
        }
    }
}

// Dockspace + main menu bar system
static void editor_dockspace(World& world) {
    auto* state = world.try_resource<EditorState>();
    auto* commands = world.try_resource<EditorCommands>();
    if (!state || !commands) return;

    // Fullscreen dockspace
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->Pos);
    ImGui::SetNextWindowSize(vp->Size);
    ImGui::SetNextWindowViewport(vp->ID);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    bool open = true;
    ImGui::Begin("HeliosEditorDockspace", &open, flags);
    ImGui::PopStyleVar(3);

    ImGuiID dockspace_id = ImGui::GetID("EditorDockSpace");

    // Build dock layout on every frame where the node doesn't exist yet.
    // With io.IniFilename=nullptr, this only triggers on the first frame.
    if (ImGui::DockBuilderGetNode(dockspace_id) == nullptr) {
        ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspace_id, vp->Size);

        // Split: left (25%) | center+right
        ImGuiID left, center_right;
        ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Left, 0.25f, &left, &center_right);

        // Split center+right: center | right (25%)
        ImGuiID center, right;
        ImGui::DockBuilderSplitNode(center_right, ImGuiDir_Right, 0.25f, &right, &center);

        // Split center: viewport (55%) | console (45%)
        ImGuiID viewport_area, console_area;
        ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.45f, &console_area, &viewport_area);

        // Right side: inspector (65% top) | stats + scene settings (35% bottom)
        ImGuiID inspector_area, right_bottom;
        ImGui::DockBuilderSplitNode(right, ImGuiDir_Down, 0.35f, &right_bottom, &inspector_area);

        // Left: scene hierarchy (50% top) | content browser (50% bottom)
        ImGuiID hierarchy_area, content_area;
        ImGui::DockBuilderSplitNode(left, ImGuiDir_Down, 0.5f, &content_area, &hierarchy_area);

        ImGui::DockBuilderDockWindow("Scene Hierarchy", hierarchy_area);
        ImGui::DockBuilderDockWindow("Content Browser", content_area);
        ImGui::DockBuilderDockWindow("Viewport", viewport_area);
        ImGui::DockBuilderDockWindow("Console", console_area);
        ImGui::DockBuilderDockWindow("Inspector", inspector_area);
        ImGui::DockBuilderDockWindow("Stats", right_bottom);
        ImGui::DockBuilderDockWindow("Scene Settings", right_bottom);

        // Set flags on each dock node: no close, no menu
        auto set_node_flags = [](ImGuiID node_id) {
            ImGuiDockNode* node = ImGui::DockBuilderGetNode(node_id);
            if (node) {
                node->LocalFlags |= ImGuiDockNodeFlags_NoCloseButton |
                                     ImGuiDockNodeFlags_NoWindowMenuButton;
            }
        };
        set_node_flags(hierarchy_area);
        set_node_flags(content_area);
        set_node_flags(viewport_area);
        set_node_flags(console_area);
        set_node_flags(inspector_area);
        set_node_flags(right_bottom);

        // Clear CentralNode flag — it causes side panels to use absolute pixel
        // sizes instead of proportional, so they don't scale with the window.
        ImGuiDockNode* central = ImGui::DockBuilderGetCentralNode(dockspace_id);
        if (central) {
            central->LocalFlags &= ~ImGuiDockNodeFlags_CentralNode;
        }

        ImGui::DockBuilderFinish(dockspace_id);
    }

    // NoResize on the dockspace prevents users from dragging splitters
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f),
        ImGuiDockNodeFlags_NoUndocking | ImGuiDockNodeFlags_NoResize);

    auto* project = world.try_resource<Project>();

    // New Project dialog state (persists across frames)
    static bool show_new_project_dialog = false;
    static char new_project_name[256] = "NewProject";
    static char new_project_dir[1024] = "";

    // Main menu bar
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem(ICON_MDI_FOLDER_PLUS " New Project...", "Ctrl+P")) {
                show_new_project_dialog = true;
                new_project_name[0] = '\0';
                strncpy(new_project_name, "NewProject", sizeof(new_project_name));
                new_project_dir[0] = '\0';
            }
            if (ImGui::MenuItem(ICON_MDI_FOLDER_OPEN " Open Project...", "Ctrl+O")) {
                nfdu8filteritem_t filters[] = {{ "Helios Project", "hveproject" }};
                nfdu8char_t* out_path = nullptr;
                if (NFD_OpenDialog(&out_path, filters, 1, nullptr) == NFD_OKAY) {
                    Project proj;
                    proj.load(out_path);
                    proj.apply_to_world(world);
                    HELIOS_LOG(Editor, Info, "Opened project: {}", proj.name);
                    world.insert_resource(std::move(proj));
                    NFD_FreePath(out_path);
                }
            }
            if (ImGui::MenuItem(ICON_MDI_CONTENT_SAVE " Save", "Ctrl+S")) {
                save_all(world);
            }
            ImGui::Separator();
            if (ImGui::MenuItem(ICON_MDI_EXIT_TO_APP " Exit")) {
                if (auto* windows = world.try_resource<Windows>()) {
                    windows->request_quit();
                }
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem(ICON_MDI_UNDO " Undo", "Ctrl+Z", false, commands->can_undo())) {
                commands->undo(world);
            }
            if (ImGui::MenuItem(ICON_MDI_REDO " Redo", "Ctrl+Y", false, commands->can_redo())) {
                commands->redo(world);
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Scene Hierarchy", nullptr, &state->show_scene_hierarchy);
            ImGui::MenuItem("Inspector", nullptr, &state->show_inspector);
            ImGui::MenuItem("Content Browser", nullptr, &state->show_content_browser);
            ImGui::MenuItem("Viewport", nullptr, &state->show_viewport);
            ImGui::MenuItem("Scene Settings", nullptr, &state->show_scene_settings);
            ImGui::MenuItem("Console", nullptr, &state->show_console);
            ImGui::MenuItem("Stats", nullptr, &state->show_stats);
            ImGui::EndMenu();
        }
        if (ImGui::MenuItem(ICON_MDI_COG " Project Settings")) {
            state->show_project_settings = true;
        }

        // Play/Stop button in menu bar
        ImGui::Separator();
        if (state->mode == EditorMode::Edit) {
            if (ImGui::MenuItem(ICON_MDI_PLAY " Play")) {
                enter_play_mode(world, *state);
            }
        } else {
            if (ImGui::MenuItem(ICON_MDI_STOP " Stop")) {
                exit_play_mode(world, *state);
            }
        }
        ImGui::EndMenuBar();
    }

    ImGui::End();

    // --- Keyboard shortcuts ---
    ImGuiIO& io = ImGui::GetIO();
    if (!io.WantTextInput) {
        // File shortcuts
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S)) {
            save_all(world);
        }
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_O)) {
            nfdu8filteritem_t filters[] = {{ "Helios Project", "hveproject" }};
            nfdu8char_t* out_path = nullptr;
            if (NFD_OpenDialog(&out_path, filters, 1, nullptr) == NFD_OKAY) {
                Project proj;
                proj.load(out_path);
                proj.apply_to_world(world);
                world.insert_resource(std::move(proj));
                NFD_FreePath(out_path);
            }
        }
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_P)) {
            show_new_project_dialog = true;
            strncpy(new_project_name, "NewProject", sizeof(new_project_name));
            new_project_dir[0] = '\0';
        }
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z)) {
            if (commands->can_undo()) commands->undo(world);
        }
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y)) {
            if (commands->can_redo()) commands->redo(world);
        }

        // Delete selected entity
        if (ImGui::IsKeyPressed(ImGuiKey_Delete)) {
            if (world.is_alive(state->selected_entity)) {
                world.despawn(state->selected_entity);
                state->selected_entity = Entity{};
            }
        }

        // Gizmo mode (only when viewport focused)
        if (state->viewport_focused) {
            if (ImGui::IsKeyPressed(ImGuiKey_W)) state->gizmo_op = GizmoOperation::Translate;
            if (ImGui::IsKeyPressed(ImGuiKey_E)) state->gizmo_op = GizmoOperation::Rotate;
            if (ImGui::IsKeyPressed(ImGuiKey_R)) state->gizmo_op = GizmoOperation::Scale;
        }

        // Play/Stop
        if (ImGui::IsKeyPressed(ImGuiKey_F5)) {
            if (state->mode == EditorMode::Edit) {
                enter_play_mode(world, *state);
            }
        }
        if (io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            if (state->mode == EditorMode::Play) {
                exit_play_mode(world, *state);
            }
        }
    }

    // --- New Project modal dialog ---
    if (show_new_project_dialog) {
        ImGui::OpenPopup("Create Project");
        show_new_project_dialog = false;
    }

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(600, 200), ImGuiCond_Appearing);

    if (ImGui::BeginPopupModal("Create Project", nullptr,
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove)) {

        ImGui::InputText("Project Name", new_project_name, sizeof(new_project_name));

        ImGui::InputText("##dir", new_project_dir, sizeof(new_project_dir));
        ImGui::SameLine();
        if (ImGui::Button("...")) {
            nfdu8char_t* out_path = nullptr;
            if (NFD_PickFolder(&out_path, nullptr) == NFD_OKAY) {
                strncpy(new_project_dir, out_path, sizeof(new_project_dir) - 1);
                new_project_dir[sizeof(new_project_dir) - 1] = '\0';
                NFD_FreePath(out_path);
            }
        }
        ImGui::SameLine();
        ImGui::Text("Project Directory");

        ImGui::Separator();

        bool name_valid = strlen(new_project_name) > 0;
        bool dir_valid = strlen(new_project_dir) > 0;

        if (!name_valid || !dir_valid) ImGui::BeginDisabled();
        if (ImGui::Button("Create", ImVec2(120, 0))) {
            if (!world.has_resource<std::unique_ptr<ScriptBuildState>>())
                world.insert_resource(std::make_unique<ScriptBuildState>());
            auto* bs = world.resource<std::unique_ptr<ScriptBuildState>>().get();
            ThreadPool* pool = nullptr;
            if (world.has_resource<std::shared_ptr<ThreadPool>>())
                pool = world.resource<std::shared_ptr<ThreadPool>>().get();
            auto proj = Project::create_new(new_project_dir, new_project_name, bs, pool);
            proj.apply_to_world(world);
            HELIOS_LOG(Editor, Info, "Created project '{}' at {}",
                       proj.name, proj.project_dir.string());
            world.insert_resource(std::move(proj));
            ImGui::CloseCurrentPopup();
        }
        if (!name_valid || !dir_valid) ImGui::EndDisabled();

        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

static void shutdown_editor(World& world) {
    // Disable scripts before shutdown
    if (auto* exec = world.try_resource<ScriptExecutionState>()) {
        exec->running = false;
    }
    NFD_Quit();
}

// Stats panel
static void stats_panel(World& world) {
    auto* state = world.try_resource<EditorState>();
    if (!state || !state->show_stats) return;

    auto* time = world.try_resource<Time>();

    ImGui::Begin("Stats");
    if (time) {
        float fps = (time->delta() > 0.0f) ? 1.0f / time->delta() : 0.0f;
        ImGui::Text("FPS: %.1f", fps);
        ImGui::Text("Frame Time: %.2f ms", time->delta() * 1000.0f);
        ImGui::Text("Frame: %u", time->frame_count());
    }

    auto* packet = world.try_resource<renderer::FramePacket>();
    if (packet) {
        ImGui::Separator();
        ImGui::Text("Draw Calls: %zu", packet->mesh_draws.size());
        ImGui::Text("Point Lights: %zu", packet->point_lights.size());
        ImGui::Text("Dir Lights: %zu", packet->dir_lights.size());
        ImGui::Text("Cameras: %zu", packet->camera_views.size());
    }

    auto* build_ptr = world.try_resource<std::unique_ptr<ScriptBuildState>>();
    if (build_ptr && *build_ptr) {
        auto status = (*build_ptr)->status.load();
        if (status == ScriptBuildState::Status::Building) {
            ImGui::Separator();
            ImGui::TextColored(ImVec4(1, 1, 0, 1), "Building scripts...");
        } else if (status == ScriptBuildState::Status::Failed) {
            ImGui::Separator();
            ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "Script build failed");
        }
    }

    ImGui::End();
}

// Runs after extract_render_data, before camera_driver.
// In Edit mode: strips non-editor cameras from the main scene target
// and injects a preview CameraView for the selected camera entity.
static void inject_camera_preview(World& world) {
    auto* state = world.try_resource<EditorState>();
    auto* ctx = world.try_resource<RenderContext>();
    auto* packet = world.try_resource<renderer::FramePacket>();
    if (!state || !ctx || !packet || !ctx->frame_active) return;

    // In Edit mode, only the EditorCamera should render to scene_color.
    // extract_render_data added ALL ActiveCamera entities — strip game cameras
    // from the default target (null = scene_color). They'll get a preview target instead.
    if (state->mode == EditorMode::Edit) {
        // Find which CameraView is the editor camera (matches EditorCamera position)
        auto editor_q = world.query<const Transform, const Camera, With<EditorOnly>>();
        glm::vec3 editor_pos{0.0f};
        for (auto [e, t, cam] : editor_q.with_entity()) {
            editor_pos = t.position;
            break;
        }

        // Remove non-editor cameras from the default scene target
        auto& views = packet->camera_views;
        views.erase(
            std::remove_if(views.begin(), views.end(), [&](const renderer::CameraView& v) {
                // Keep cameras with custom targets (they're not scene_color)
                if (v.target_color != nullptr) return false;
                // Keep the editor camera (match by position)
                return v.camera.position != editor_pos;
            }),
            views.end());

        // Update primary camera reference
        if (!views.empty()) {
            packet->camera = views.front().camera;
        }
    }

    Entity entity = state->selected_entity;
    if (!world.is_alive(entity)) return;

    auto* cam = world.try_get<Camera>(entity);
    auto* tf = world.try_get<Transform>(entity);
    if (!cam || !tf) return;

    // Don't preview the editor camera itself
    auto* tag = world.try_get<Tag>(entity);
    if (world.has<EditorOnly>(entity)) return;

    // Get or create the preview render target
    auto [color, depth] = ctx->get_or_create_target(
        state->camera_preview_target_id,
        state->camera_preview_width,
        state->camera_preview_height);

    float aspect = static_cast<float>(state->camera_preview_width)
                 / static_cast<float>(state->camera_preview_height);

    renderer::CameraView preview;
    preview.camera = renderer::CameraData{
        .view = glm::inverse(tf->to_mat4()),
        .projection = glm::perspective(
            glm::radians(cam->fov_degrees), aspect,
            cam->near_plane, cam->far_plane),
        .position = tf->position,
        .near_plane = cam->near_plane,
        .far_plane = cam->far_plane,
        .fov_y = cam->fov_degrees,
        .aspect_ratio = aspect,
    };
    preview.target_color = color;
    preview.target_depth = depth;
    preview.target_width = state->camera_preview_width;
    preview.target_height = state->camera_preview_height;
    preview.clear_mode = renderer::CameraClearMode::SolidColor;
    preview.layer_mask = 0xFFFFFFFF; // see everything

    packet->camera_views.push_back(preview);
}

// All panel systems as a single dispatching system (panels are not thread-safe
// since they all write to ImGui global state)
static void editor_panels(World& world) {
    scene_hierarchy_panel(world);
    inspector_panel(world);
    content_browser_panel(world);
    viewport_panel(world);
    project_settings_panel(world);
    scene_settings_panel(world);
    stats_panel(world);
    console_panel(world);
}

void EditorPlugin::build(App& app) {
    // ImGui rendering
    app.add_plugin(ImGuiRenderPlugin{});

    // Editor state
    EditorState state{};
    state.project_path = config.project_path;
    if (!config.project_path.empty()) {
        state.project_name = config.project_path.stem().string();
    }
    app.insert_resource(std::move(state));
    app.insert_resource(EditorCommands{});
    app.insert_resource(EditorResources{});
    app.insert_resource(SceneEditState{});

    // Scene serializer with all registered component types
    SceneSerializer serializer;
    serializer.register_component<Tag>("Tag",
        [](YAML::Emitter& out, const Tag& t) { serialize_yaml(out, t); },
        [](const YAML::Node& n) { return deserialize_yaml_tag(n); });
    serializer.register_component<Transform>("Transform",
        [](YAML::Emitter& out, const Transform& t) { serialize_yaml(out, t); },
        [](const YAML::Node& n) { return deserialize_yaml_transform(n); });
    serializer.register_component<Camera>("Camera",
        [](YAML::Emitter& out, const Camera& c) { serialize_yaml(out, c); },
        [](const YAML::Node& n) { return deserialize_yaml_camera(n); });
    serializer.register_component<ActiveCamera>("ActiveCamera",
        [](YAML::Emitter& out, const ActiveCamera& ac) { serialize_yaml(out, ac); },
        [](const YAML::Node& n) { return deserialize_yaml_active_camera(n); });
    serializer.register_component<MeshRenderer>("MeshRenderer",
        [](YAML::Emitter& out, const MeshRenderer& mr) { serialize_yaml(out, mr); },
        [](const YAML::Node& n) { return deserialize_yaml_mesh_renderer(n); });
    serializer.register_component<PointLight>("PointLight",
        [](YAML::Emitter& out, const PointLight& pl) { serialize_yaml(out, pl); },
        [](const YAML::Node& n) { return deserialize_yaml_point_light(n); });
    serializer.register_component<DirectionalLight>("DirectionalLight",
        [](YAML::Emitter& out, const DirectionalLight& dl) { serialize_yaml(out, dl); },
        [](const YAML::Node& n) { return deserialize_yaml_directional_light(n); });
    serializer.register_component<RigidBody>("RigidBody",
        [](YAML::Emitter& out, const RigidBody& rb) { serialize_yaml(out, rb); },
        [](const YAML::Node& n) { return deserialize_yaml_rigid_body(n); });
    serializer.register_component<BoxCollider>("BoxCollider",
        [](YAML::Emitter& out, const BoxCollider& bc) { serialize_yaml(out, bc); },
        [](const YAML::Node& n) { return deserialize_yaml_box_collider(n); });
    serializer.register_component<SphereCollider>("SphereCollider",
        [](YAML::Emitter& out, const SphereCollider& sc) { serialize_yaml(out, sc); },
        [](const YAML::Node& n) { return deserialize_yaml_sphere_collider(n); });
    serializer.register_component<AudioSource>("AudioSource",
        [](YAML::Emitter& out, const AudioSource& as) { serialize_yaml(out, as); },
        [](const YAML::Node& n) { return deserialize_yaml_audio_source(n); });
    serializer.register_component<RenderLayers>("RenderLayers",
        [](YAML::Emitter& out, const RenderLayers& rl) { serialize_yaml(out, rl); },
        [](const YAML::Node& n) { return deserialize_yaml_render_layers(n); });
    serializer.register_component<SceneRoot>("SceneRoot",
        [](YAML::Emitter& out, const SceneRoot& sr) { serialize_yaml(out, sr); },
        [](const YAML::Node& n) { return deserialize_yaml_scene_root(n); });
    serializer.register_component<ScriptInstance>("Script",
        [](YAML::Emitter& out, const ScriptInstance& si) {
            out << YAML::BeginMap;
            out << YAML::Key << "class" << YAML::Value << si.script_class_name;
            out << YAML::EndMap;
        },
        [](const YAML::Node& n) {
            ScriptInstance si;
            if (n["class"]) si.script_class_name = n["class"].as<std::string>();
            return si;
        });
    app.insert_resource(std::move(serializer));

    // Load project if path was provided
    if (!config.project_path.empty() && std::filesystem::exists(config.project_path)) {
        Project project;
        project.load(config.project_path);

        // Initialize scripting from project's assembly path
        if (!project.script_assembly.empty()) {
            auto proj_dir = project.project_dir;
            auto assembly = proj_dir / project.script_assembly;
            auto scriptcore_dll = std::filesystem::current_path() /
                "ScriptCore" / "bin" / "Release" / "net10.0" / "ScriptCore.dll";
            auto runtimeconfig = std::filesystem::current_path() /
                "ScriptCore" / "ScriptCore.runtimeconfig.json";

            ScriptingPlugin scripting;
            scripting.config.runtime_config_path = runtimeconfig;
            scripting.config.script_core_dll_path = scriptcore_dll;
            if (std::filesystem::exists(assembly))
                scripting.config.app_assembly_path = assembly;
            scripting.config.watch_directory = proj_dir / "Scripts";
            app.add_plugin(std::move(scripting));
        }

        project.apply_to_world(app.world());
        app.insert_resource(std::move(project));
    } else {
        app.insert_resource(Project{});
    }

    // No physics world in Edit mode — systems are no-ops when the ptr is null
    if (auto* pw = app.world().try_resource<std::unique_ptr<physics::PhysicsWorld>>()) {
        pw->reset();
    }

    // Scripts don't run in Edit mode (ScriptExecutionState defaults to running=false)

    // Editor camera
    app.add_system(Schedule::Startup, setup_editor_camera, "setup_editor_camera");
    app.add_system(Schedule::Startup, load_editor_resources, "load_editor_resources");

    // Bridge ECS colliders → physics::Collider (before physics_auto_create)
    app.add_system(Schedule::PreUpdate, sync_colliders_to_physics, "sync_colliders_to_physics")
        .before(app.id_of("physics_auto_create"));

    // Editor camera update
    app.add_system(Schedule::Update, update_editor_camera, "update_editor_camera");

    // Camera preview injection: runs after extract but before camera_driver
    auto extract_id = app.id_of("extract_render_data");
    auto driver_id = app.id_of("camera_driver");
    app.add_system(Schedule::PreRender, inject_camera_preview, "inject_camera_preview")
        .after(extract_id).before(driver_id);

    // Dockspace + panels (PreRender, between imgui begin/end)
    auto begin_id = app.id_of("begin_imgui_frame");
    auto end_id = app.id_of("end_imgui_frame");

    auto dock_id = app.add_system(Schedule::PreRender, editor_dockspace, "editor_dockspace")
        .after(begin_id).before(end_id).id();

    app.add_system(Schedule::PreRender, editor_panels, "editor_panels")
        .after(dock_id).before(end_id);

    // Initialize native file dialog library
    NFD_Init();

    app.add_system(Schedule::Shutdown, shutdown_editor, "shutdown_editor");

    HELIOS_LOG(Editor, Info, "EditorPlugin registered");
}

} // namespace helios::editor
