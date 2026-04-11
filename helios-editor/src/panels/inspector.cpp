#include "inspector.h"
#include "../editor_state.h"

#include <helios/ecs/world.h>
#include <helios/components/components.h>
#include <helios/script/script_instance.h>
#include <helios/script/script_runtime.h>
#include <helios/assets/asset_server.h>
#include <helios/assets/asset_binary.h>
#include <helios/assets/mesh_asset.h>

#include "../editor_utils.h"

#include <imgui.h>
#include <glm/gtc/type_ptr.hpp>
#include "../icons/IconsMaterialDesignIcons.h"

#include <filesystem>

namespace helios::editor {

// ---- Remove component context menu ----
// Call after ImGui::CollapsingHeader. Adds right-click "Remove" option.
template<typename T>
static void remove_component_menu(World& world, Entity entity) {
    if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem(ICON_MDI_DELETE " Remove Component")) {
            world.remove<T>(entity);
        }
        ImGui::EndPopup();
    }
}

static bool draw_vec3(const char* label, glm::vec3& v) {
    bool changed = false;
    ImGui::PushID(label);
    begin_field(label);

    float width = (ImGui::GetContentRegionAvail().x - 8.0f) / 3.0f;
    ImGui::PushItemWidth(width);
    changed |= ImGui::DragFloat("##X", &v.x, 0.1f);
    ImGui::SameLine();
    changed |= ImGui::DragFloat("##Y", &v.y, 0.1f);
    ImGui::SameLine();
    changed |= ImGui::DragFloat("##Z", &v.z, 0.1f);
    ImGui::PopItemWidth();

    end_field();
    ImGui::PopID();
    return changed;
}

static bool draw_float(const char* label, float& v, float speed = 0.1f, float min = 0.0f, float max = 0.0f) {
    ImGui::PushID(label);
    begin_field(label);
    ImGui::SetNextItemWidth(-1);
    bool changed = ImGui::DragFloat("##v", &v, speed, min, max);
    end_field();
    ImGui::PopID();
    return changed;
}

static bool draw_int(const char* label, int32_t& v) {
    ImGui::PushID(label);
    begin_field(label);
    ImGui::SetNextItemWidth(-1);
    bool changed = ImGui::DragInt("##v", &v);
    end_field();
    ImGui::PopID();
    return changed;
}

static bool draw_color3(const char* label, glm::vec3& v) {
    ImGui::PushID(label);
    begin_field(label);
    ImGui::SetNextItemWidth(-1);
    bool changed = ImGui::ColorEdit3("##v", glm::value_ptr(v));
    end_field();
    ImGui::PopID();
    return changed;
}

static bool draw_combo(const char* label, int& v, const char* const items[], int count) {
    ImGui::PushID(label);
    begin_field(label);
    ImGui::SetNextItemWidth(-1);
    bool changed = ImGui::Combo("##v", &v, items, count);
    end_field();
    ImGui::PopID();
    return changed;
}

static bool draw_checkbox(const char* label, bool& v) {
    ImGui::PushID(label);
    begin_field(label);
    bool changed = ImGui::Checkbox("##v", &v);
    end_field();
    ImGui::PopID();
    return changed;
}

// ---- Component drawers ----

static void draw_tag_component(World& world, Entity entity) {
    auto* tag = world.try_get<Tag>(entity);
    if (!tag) return;

    char buf[256];
    strncpy(buf, tag->name.c_str(), sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    if (ImGui::InputText("##Tag", buf, sizeof(buf))) {
        tag->name = buf;
    }
    ImGui::Separator();
}

static void draw_transform_component(World& world, Entity entity) {
    auto* t = world.try_get<Transform>(entity);
    if (!t) return;

    if (ImGui::CollapsingHeader(ICON_MDI_AXIS_ARROW " Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
        draw_vec3("Position", t->position);

        glm::vec3 euler = glm::degrees(glm::eulerAngles(t->rotation));
        if (draw_vec3("Rotation", euler)) {
            t->rotation = glm::quat(glm::radians(euler));
        }

        draw_vec3("Scale", t->scale);
    }
}

static void draw_camera_component(World& world, Entity entity) {
    auto* cam = world.try_get<Camera>(entity);
    if (!cam) return;

    if (ImGui::CollapsingHeader(ICON_MDI_VIDEO " Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
        remove_component_menu<Camera>(world, entity);
        const char* proj_types[] = {"Perspective", "Orthographic"};
        int proj = static_cast<int>(cam->projection);
        draw_combo("Projection", proj, proj_types, 2);
        cam->projection = static_cast<ProjectionType>(proj);

        draw_float("FOV", cam->fov_degrees, 1.0f, 1.0f, 179.0f);
        draw_float("Near", cam->near_plane, 0.01f, 0.001f, 100.0f);
        draw_float("Far", cam->far_plane, 1.0f, 1.0f, 100000.0f);
        draw_int("Order", cam->order);

        bool is_primary = world.has<ActiveCamera>(entity);
        if (draw_checkbox("Primary", is_primary)) {
            if (is_primary) world.add(entity, ActiveCamera{});
            else world.remove<ActiveCamera>(entity);
        }
    }
}

static void draw_point_light_component(World& world, Entity entity) {
    auto* light = world.try_get<PointLight>(entity);
    if (!light) return;

    if (ImGui::CollapsingHeader(ICON_MDI_LIGHTBULB " Point Light", ImGuiTreeNodeFlags_DefaultOpen)) {
        remove_component_menu<PointLight>(world, entity);
        draw_color3("Color", light->color);
        draw_float("Intensity", light->intensity, 0.1f, 0.0f, 100.0f);
        draw_float("Radius", light->radius, 0.1f, 0.0f, 1000.0f);
    }
}

static void draw_directional_light_component(World& world, Entity entity) {
    auto* light = world.try_get<DirectionalLight>(entity);
    if (!light) return;

    if (ImGui::CollapsingHeader(ICON_MDI_WHITE_BALANCE_SUNNY " Directional Light", ImGuiTreeNodeFlags_DefaultOpen)) {
        remove_component_menu<DirectionalLight>(world, entity);
        draw_color3("Color", light->color);
        draw_float("Intensity", light->intensity, 0.1f, 0.0f, 100.0f);
    }
}

static void draw_mesh_renderer_component(World& world, Entity entity) {
    auto* mr = world.try_get<MeshRenderer>(entity);
    if (!mr) return;

    if (ImGui::CollapsingHeader(ICON_MDI_CUBE_OUTLINE " Mesh Renderer", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::PushID("MeshRenderer");
        remove_component_menu<MeshRenderer>(world, entity);

        // Mesh field with drag-drop target
        begin_field("Mesh");
        // Show mesh handle info
        std::string mesh_label;
        if (mr->mesh) {
            auto handle = mr->mesh.untyped();
            mesh_label = "Mesh #" + std::to_string(handle.index) + "##mesh_slot";
        } else {
            mesh_label = "[Drop mesh here]##mesh_slot";
        }
        if (!mr->mesh_path.empty()) {
            mesh_label = std::filesystem::path(mr->mesh_path).stem().string() + "##mesh_slot";
        }
        ImGui::Button(mesh_label.c_str(), ImVec2(-1, 0));
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
                std::string path(static_cast<const char*>(payload->Data));
                std::filesystem::path p(path);
                auto ext = p.extension().string();
                bool is_mesh = (ext == ".hvemesh" || ext == ".glb" || ext == ".gltf"
                             || ext == ".fbx" || ext == ".obj");
                if (is_mesh && world.has_resource<std::shared_ptr<AssetServer>>()) {
                    auto& server = world.resource<std::shared_ptr<AssetServer>>();
                    auto rel = make_relative(path, server->root());

                    // Auto-import source files to .hvemesh
                    if (ext != ".hvemesh") {
                        auto hvemesh_rel = std::filesystem::path(rel)
                            .replace_extension(".hvemesh").string();
                        if (!std::filesystem::exists(server->root() / hvemesh_rel)) {
                            server->import_asset(server->root() / rel,
                                AssetBinaryType::Mesh, hvemesh_rel);
                        }
                        rel = hvemesh_rel;
                    }

                    mr->mesh = server->load<MeshAsset>(rel);
                    mr->mesh_path = rel;
                }
            }
            ImGui::EndDragDropTarget();
        }
        end_field();

        // Material field with drag-drop target (future: material assets)
        begin_field("Material");
        ImGui::Text("%s", mr->material ? "Loaded" : "Default");
        end_field();

        draw_int("Sort Order", mr->sort_order);

        bool cast_shadows = Flags::has(mr->flags, MeshFlags::CastShadows);
        if (draw_checkbox("Cast Shadows", cast_shadows)) {
            if (cast_shadows) Flags::set(mr->flags, MeshFlags::CastShadows);
            else Flags::clear(mr->flags, MeshFlags::CastShadows);
        }

        bool recv_shadows = Flags::has(mr->flags, MeshFlags::ReceiveShadows);
        if (draw_checkbox("Recv Shadows", recv_shadows)) {
            if (recv_shadows) Flags::set(mr->flags, MeshFlags::ReceiveShadows);
            else Flags::clear(mr->flags, MeshFlags::ReceiveShadows);
        }
        ImGui::PopID();
    }
}

static void draw_rigidbody_component(World& world, Entity entity) {
    auto* rb = world.try_get<RigidBody>(entity);
    if (!rb) return;

    if (ImGui::CollapsingHeader(ICON_MDI_SOCCER " Rigid Body", ImGuiTreeNodeFlags_DefaultOpen)) {
        const char* types[] = {"Static", "Kinematic", "Dynamic"};
        int type = static_cast<int>(rb->body_type);
        remove_component_menu<RigidBody>(world, entity);
        if (draw_combo("Body Type", type, types, 3)) {
            rb->body_type = static_cast<BodyType>(type);
        }
        draw_float("Mass", rb->mass, 0.1f, 0.0f, 10000.0f);
        draw_float("Friction", rb->friction, 0.01f, 0.0f, 1.0f);
        draw_float("Restitution", rb->restitution, 0.01f, 0.0f, 1.0f);

        bool gravity = Flags::has(rb->flags, RigidBodyFlags::UseGravity);
        if (draw_checkbox("Use Gravity", gravity)) {
            if (gravity) Flags::set(rb->flags, RigidBodyFlags::UseGravity);
            else Flags::clear(rb->flags, RigidBodyFlags::UseGravity);
        }
    }
}

static void draw_box_collider_component(World& world, Entity entity) {
    auto* col = world.try_get<BoxCollider>(entity);
    if (!col) return;

    if (ImGui::CollapsingHeader(ICON_MDI_CUBE_OUTLINE " Box Collider", ImGuiTreeNodeFlags_DefaultOpen)) {
        remove_component_menu<BoxCollider>(world, entity);
        draw_vec3("Half Extents", col->half_extents);
        draw_vec3("Offset", col->offset);
        bool trigger = Flags::has(col->flags, ColliderFlags::IsTrigger);
        if (draw_checkbox("Is Trigger", trigger)) {
            if (trigger) Flags::set(col->flags, ColliderFlags::IsTrigger);
            else Flags::clear(col->flags, ColliderFlags::IsTrigger);
        }
    }
}

static void draw_sphere_collider_component(World& world, Entity entity) {
    auto* col = world.try_get<SphereCollider>(entity);
    if (!col) return;

    if (ImGui::CollapsingHeader(ICON_MDI_CIRCLE_OUTLINE " Sphere Collider", ImGuiTreeNodeFlags_DefaultOpen)) {
        remove_component_menu<SphereCollider>(world, entity);
        draw_float("Radius", col->radius, 0.01f, 0.0f, 1000.0f);
        draw_vec3("Offset", col->offset);

        bool trigger = Flags::has(col->flags, ColliderFlags::IsTrigger);
        if (draw_checkbox("Is Trigger", trigger)) {
            if (trigger) Flags::set(col->flags, ColliderFlags::IsTrigger);
            else Flags::clear(col->flags, ColliderFlags::IsTrigger);
        }
    }
}

static void draw_audio_source_component(World& world, Entity entity) {
    auto* src = world.try_get<AudioSource>(entity);
    if (!src) return;

    if (ImGui::CollapsingHeader(ICON_MDI_MUSIC_NOTE " Audio Source", ImGuiTreeNodeFlags_DefaultOpen)) {
        remove_component_menu<AudioSource>(world, entity);
        draw_float("Volume", src->volume, 0.01f, 0.0f, 1.0f);
        draw_float("Pitch", src->pitch, 0.01f, 0.1f, 4.0f);

        bool looping = Flags::has(src->flags, AudioFlags::Looping);
        if (draw_checkbox("Looping", looping)) {
            if (looping) Flags::set(src->flags, AudioFlags::Looping);
            else Flags::clear(src->flags, AudioFlags::Looping);
        }

        bool play_on_start = Flags::has(src->flags, AudioFlags::PlayOnStart);
        if (draw_checkbox("Play on Start", play_on_start)) {
            if (play_on_start) Flags::set(src->flags, AudioFlags::PlayOnStart);
            else Flags::clear(src->flags, AudioFlags::PlayOnStart);
        }
    }
}

static void draw_script_component(World& world, Entity entity) {
    auto* script = world.try_get<ScriptInstance>(entity);
    if (!script) return;

    if (ImGui::CollapsingHeader(ICON_MDI_LANGUAGE_CSHARP " Script", ImGuiTreeNodeFlags_DefaultOpen)) {
        remove_component_menu<ScriptInstance>(world, entity);
        ImGui::PushID("Script");

        // Get available class names from the runtime (always alive now)
        ScriptRuntime* rt = nullptr;
        if (auto* active = world.try_resource<std::unique_ptr<ScriptRuntime>>()) {
            if (*active) rt = active->get();
        }

        begin_field("Class");
        ImGui::SetNextItemWidth(-1);

        if (rt) {
            auto classes = rt->get_script_class_names();
            // Find current selection index
            int current = -1;
            for (int i = 0; i < static_cast<int>(classes.size()); i++) {
                if (classes[i] == script->script_class_name) { current = i; break; }
            }

            // Combo dropdown
            const char* preview = current >= 0
                ? classes[current].c_str()
                : (script->script_class_name.empty() ? "[Select Class]" : script->script_class_name.c_str());

            if (ImGui::BeginCombo("##class", preview)) {
                for (int i = 0; i < static_cast<int>(classes.size()); i++) {
                    bool selected = (i == current);
                    if (ImGui::Selectable(classes[i].c_str(), selected)) {
                        script->script_class_name = classes[i];
                    }
                    if (selected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
        } else {
            // Fallback: manual text input when no runtime available
            char buf[256];
            strncpy(buf, script->script_class_name.c_str(), sizeof(buf) - 1);
            buf[sizeof(buf) - 1] = '\0';
            if (ImGui::InputText("##v", buf, sizeof(buf))) {
                script->script_class_name = buf;
            }
        }

        end_field();
        ImGui::PopID();
    }
}

// ---- Main panel ----

void inspector_panel(World& world) {
    auto* state = world.try_resource<EditorState>();
    if (!state || !state->show_inspector) return;

    ImGui::Begin("Inspector");

    Entity entity = state->selected_entity;
    if (!world.is_alive(entity)) {
        ImGui::Text("No entity selected");
        ImGui::End();
        return;
    }

    draw_tag_component(world, entity);
    draw_transform_component(world, entity);
    draw_camera_component(world, entity);
    draw_point_light_component(world, entity);
    draw_directional_light_component(world, entity);
    draw_mesh_renderer_component(world, entity);
    draw_rigidbody_component(world, entity);
    draw_box_collider_component(world, entity);
    draw_sphere_collider_component(world, entity);
    draw_audio_source_component(world, entity);
    draw_script_component(world, entity);

    // Add Component button
    ImGui::Separator();
    float avail = ImGui::GetContentRegionAvail().x;
    ImGui::SetCursorPosX((avail - 200.0f) * 0.5f);
    if (ImGui::Button(ICON_MDI_PLUS " Add Component", ImVec2(200, 0))) {
        ImGui::OpenPopup("AddComponent");
    }

    if (ImGui::BeginPopup("AddComponent")) {
        if (!world.has<Camera>(entity) && ImGui::MenuItem(ICON_MDI_VIDEO " Camera")) {
            world.add(entity, Camera{});
        }
        if (!world.has<MeshRenderer>(entity) && ImGui::MenuItem(ICON_MDI_CUBE_OUTLINE " Mesh Renderer")) {
            world.add(entity, MeshRenderer{});
        }
        if (!world.has<PointLight>(entity) && ImGui::MenuItem(ICON_MDI_LIGHTBULB " Point Light")) {
            world.add(entity, PointLight{});
        }
        if (!world.has<DirectionalLight>(entity) && ImGui::MenuItem(ICON_MDI_WHITE_BALANCE_SUNNY " Directional Light")) {
            world.add(entity, DirectionalLight{});
        }
        if (!world.has<RigidBody>(entity) && ImGui::MenuItem(ICON_MDI_SOCCER " Rigid Body")) {
            world.add(entity, RigidBody{});
        }
        if (!world.has<BoxCollider>(entity) && ImGui::MenuItem(ICON_MDI_CUBE_OUTLINE " Box Collider")) {
            world.add(entity, BoxCollider{});
        }
        if (!world.has<SphereCollider>(entity) && ImGui::MenuItem(ICON_MDI_CIRCLE_OUTLINE " Sphere Collider")) {
            world.add(entity, SphereCollider{});
        }
        if (!world.has<AudioSource>(entity) && ImGui::MenuItem(ICON_MDI_MUSIC_NOTE " Audio Source")) {
            world.add(entity, AudioSource{});
        }
        if (!world.has<ScriptInstance>(entity) && ImGui::MenuItem(ICON_MDI_LANGUAGE_CSHARP " Script")) {
            world.add(entity, ScriptInstance{});
        }
        ImGui::EndPopup();
    }

    ImGui::End();
}

} // namespace helios::editor
