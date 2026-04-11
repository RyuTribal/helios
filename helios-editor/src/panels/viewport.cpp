#include "viewport.h"
#include "../editor_state.h"
#include "../gizmo/gizmo_system.h"
#include "../imgui/imgui_render_plugin.h"

#include <helios/ecs/world.h>
#include <helios/ecs/hierarchy.h>
#include <helios/components/components.h>
#include <helios/render_plugin.h>
#include <helios/assets/asset_utils.h>
#include <helios/assets/asset_binary.h>
#include <helios/serialization/scene_serializer.h>

#include <imgui.h>
#include <ImGuizmo.h>
#include <glm/gtc/matrix_transform.hpp>

#include "../editor_utils.h"

#include <cmath>
#include <filesystem>
#include <limits>
#include <string>

namespace helios::editor {

static constexpr uint32_t MAX_FIF = 3;
static ImTextureID s_scene_ds[MAX_FIF]{};
static rhi::Texture* s_scene_tex[MAX_FIF]{};
static ImTextureID s_preview_ds{};
static rhi::Texture* s_preview_last = nullptr;
static std::string s_scene_drop_path;  // pending .hvescn drop for popup choice

// Ray-AABB intersection (slab method). Returns true if hit, sets t_out to distance.
static bool ray_aabb(const glm::vec3& origin, const glm::vec3& dir,
                     const glm::vec3& box_min, const glm::vec3& box_max,
                     float& t_out) {
    float tmin = 0.0f;
    float tmax = std::numeric_limits<float>::max();
    for (int i = 0; i < 3; i++) {
        if (std::abs(dir[i]) < 1e-8f) {
            if (origin[i] < box_min[i] || origin[i] > box_max[i]) return false;
        } else {
            float t1 = (box_min[i] - origin[i]) / dir[i];
            float t2 = (box_max[i] - origin[i]) / dir[i];
            if (t1 > t2) std::swap(t1, t2);
            tmin = std::max(tmin, t1);
            tmax = std::min(tmax, t2);
            if (tmin > tmax) return false;
        }
    }
    t_out = tmin;
    return true;
}

void viewport_panel(World& world) {
    auto* state = world.try_resource<EditorState>();
    if (!state || !state->show_viewport) return;

    auto* ctx = world.try_resource<RenderContext>();
    auto* edit_state = world.try_resource<SceneEditState>();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("Viewport");

    state->viewport_focused = ImGui::IsWindowFocused();
    state->viewport_hovered = ImGui::IsWindowHovered();

    ImVec2 size = ImGui::GetContentRegionAvail();
    state->viewport_width = static_cast<uint32_t>(size.x);
    state->viewport_height = static_cast<uint32_t>(size.y);

    // Display the scene framebuffer
    bool image_drawn = false;
    if (ctx && ctx->scene_color) {
        uint32_t fi = ctx->current_frame_index;
        if (ctx->scene_color != s_scene_tex[fi]) {
            s_scene_ds[fi] = imgui_register_texture(*ctx->scene_color);
            s_scene_tex[fi] = ctx->scene_color;
        }

        if (s_scene_ds[fi] && size.x > 0 && size.y > 0) {
            ImGui::Image(s_scene_ds[fi], size);
            image_drawn = true;
        }
    }

    // Capture image rect before gizmo runs (gizmo doesn't push ImGui items)
    ImVec2 img_rect_min = ImGui::GetItemRectMin();
    ImVec2 img_rect_size = ImGui::GetItemRectSize();
    bool img_hovered = image_drawn && ImGui::IsItemHovered();

    // Set gizmo rect to match the scene image exactly (not the full window
    // which includes the title bar — that offset breaks gizmo hit-testing)
    ImGuizmo::SetRect(img_rect_min.x, img_rect_min.y, img_rect_size.x, img_rect_size.y);

    // Gizmo must run BEFORE picking so IsOver()/IsUsing() are up-to-date
    gizmo_system(world);

    // ---- Mouse picking: click to select entity ----
    if (img_hovered
        && ImGui::IsMouseClicked(ImGuiMouseButton_Left)
        && !ImGuizmo::IsOver() && !ImGuizmo::IsUsing()) {

        // Build camera view/projection
        glm::mat4 view{1.0f}, proj{1.0f};
        glm::vec3 cam_pos{0.0f};
        auto cam_q = world.query<const Transform, const Camera, With<EditorOnly>>();
        for (auto [e, ct, cam] : cam_q.with_entity()) {
            {
                cam_pos = ct.position;
                view = glm::inverse(ct.to_mat4());
                float aspect = (size.y > 0) ? size.x / size.y : 16.0f / 9.0f;
                proj = glm::perspective(glm::radians(cam.fov_degrees), aspect,
                                         cam.near_plane, cam.far_plane);
                break;
            }
        }

        // Mouse position relative to image → NDC
        ImVec2 mouse = ImGui::GetMousePos();
        float mx = (mouse.x - img_rect_min.x) / img_rect_size.x;
        float my = (mouse.y - img_rect_min.y) / img_rect_size.y;
        float ndc_x = mx * 2.0f - 1.0f;
        float ndc_y = 1.0f - my * 2.0f;

        // Unproject to world-space ray
        glm::mat4 inv_vp = glm::inverse(proj * view);
        glm::vec4 near_h = inv_vp * glm::vec4(ndc_x, ndc_y, -1.0f, 1.0f);
        glm::vec4 far_h  = inv_vp * glm::vec4(ndc_x, ndc_y,  1.0f, 1.0f);
        glm::vec3 near_w = glm::vec3(near_h) / near_h.w;
        glm::vec3 far_w  = glm::vec3(far_h) / far_h.w;
        glm::vec3 ray_dir = glm::normalize(far_w - near_w);

        // Test ray against all mesh entities' world-space AABBs
        AssetServer* server = nullptr;
        if (world.has_resource<std::shared_ptr<AssetServer>>()) {
            server = world.resource<std::shared_ptr<AssetServer>>().get();
        }

        Entity closest{};
        float closest_t = std::numeric_limits<float>::max();

        auto mesh_q = world.query<const Transform, const MeshRenderer>();
        for (auto [e, tf, mr] : mesh_q.with_entity()) {
            glm::vec3 local_min(-0.5f), local_max(0.5f);
            if (server && mr.mesh) {
                auto* mesh = server->get<MeshAsset>(mr.mesh.untyped());
                if (mesh && !mesh->vertices.empty()) {
                    local_min = local_max = mesh->vertices[0].position;
                    for (auto& v : mesh->vertices) {
                        local_min = glm::min(local_min, v.position);
                        local_max = glm::max(local_max, v.position);
                    }
                }
            }

            // Transform AABB corners to world space
            glm::mat4 model = tf.to_mat4();
            glm::vec3 w_min(std::numeric_limits<float>::max());
            glm::vec3 w_max(std::numeric_limits<float>::lowest());
            for (int i = 0; i < 8; i++) {
                glm::vec3 corner(
                    (i & 1) ? local_max.x : local_min.x,
                    (i & 2) ? local_max.y : local_min.y,
                    (i & 4) ? local_max.z : local_min.z);
                glm::vec3 wc = glm::vec3(model * glm::vec4(corner, 1.0f));
                w_min = glm::min(w_min, wc);
                w_max = glm::max(w_max, wc);
            }

            float t;
            if (ray_aabb(cam_pos, ray_dir, w_min, w_max, t) && t < closest_t) {
                closest_t = t;
                closest = e;
            }
        }

        state->selected_entity = closest;  // deselects if nothing hit
    }

    // Show active scene name as overlay in top-left of viewport
    if (edit_state && world.is_alive(edit_state->active_scene)) {
        auto* sr = world.try_get<SceneRoot>(edit_state->active_scene);
        if (sr) {
            ImVec2 overlay_pos = ImGui::GetWindowPos();
            overlay_pos.x += 8;
            overlay_pos.y += ImGui::GetFrameHeight() + 4;
            ImGui::GetWindowDrawList()->AddText(overlay_pos,
                IM_COL32(255, 255, 255, 180), sr->scene_name.c_str());
        }
    }

    // Camera preview overlay (bottom-right corner, Edit mode only)
    bool have_preview = false;
    if (state->mode == EditorMode::Edit) {
        Entity sel = state->selected_entity;
        if (world.is_alive(sel) && world.has<Camera>(sel) && !world.has<EditorOnly>(sel))
            have_preview = true;
    }

    if (have_preview && ctx) {
        auto it = ctx->camera_targets.find(state->camera_preview_target_id);
        if (it != ctx->camera_targets.end() && it->second.color) {
            auto* preview_tex = it->second.color.get();
            if (preview_tex != s_preview_last) {
                s_preview_ds = imgui_register_texture(*preview_tex);
                s_preview_last = preview_tex;
            }
        }
    }

    if (s_preview_ds && have_preview) {
        float pw = static_cast<float>(state->camera_preview_width);
        float ph = static_cast<float>(state->camera_preview_height);
        ImVec2 win_max = ImGui::GetWindowPos();
        win_max.x += ImGui::GetWindowSize().x;
        win_max.y += ImGui::GetWindowSize().y;

        ImVec2 preview_pos(win_max.x - pw - 8, win_max.y - ph - 8);
        ImGui::GetWindowDrawList()->AddRectFilled(
            ImVec2(preview_pos.x - 2, preview_pos.y - 18),
            ImVec2(preview_pos.x + pw + 2, preview_pos.y + ph + 2),
            IM_COL32(30, 30, 30, 200), 4.0f);
        ImGui::GetWindowDrawList()->AddText(
            ImVec2(preview_pos.x, preview_pos.y - 16),
            IM_COL32(255, 255, 255, 200), "Camera Preview");
        ImGui::GetWindowDrawList()->AddImage(
            s_preview_ds,
            preview_pos, ImVec2(preview_pos.x + pw, preview_pos.y + ph));
    }

    // Accept asset drops from content browser
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
            std::string path(static_cast<const char*>(payload->Data));
            std::filesystem::path p(path);
            auto ext = p.extension().string();

            // Scene file → stash path and open choice popup
            if (ext == ".hvescn") {
                s_scene_drop_path = path;
                ImGui::OpenPopup("SceneDropChoice");
            }

            // Mesh file → create entity under active scene
            bool is_mesh = (ext == ".hvemesh" || ext == ".glb" || ext == ".gltf"
                         || ext == ".fbx" || ext == ".obj");
            if (is_mesh && world.has_resource<std::shared_ptr<AssetServer>>()) {
                auto& server = world.resource<std::shared_ptr<AssetServer>>();
                auto name = p.stem().string();
                auto rel = make_relative(path, server->root());

                if (ext != ".hvemesh") {
                    auto hvemesh_rel = std::filesystem::path(rel).replace_extension(".hvemesh").string();
                    if (!std::filesystem::exists(server->root() / hvemesh_rel)) {
                        server->import_asset(server->root() / rel,
                            AssetBinaryType::Mesh, hvemesh_rel);
                    }
                    rel = hvemesh_rel;
                }

                MeshRenderer mr;
                mr.mesh = server->load<MeshAsset>(rel);
                mr.mesh_path = rel;

                auto e = world.spawn(Tag{.name = name}, Transform{}, std::move(mr));

                if (edit_state && world.is_alive(edit_state->active_scene))
                    set_parent(world, e, edit_state->active_scene);

                state->selected_entity = e;
            }
        }
        ImGui::EndDragDropTarget();
    }

    // ---- Scene drop choice popup ----
    if (ImGui::BeginPopup("SceneDropChoice")) {
        ImGui::Text("Load '%s' as:", std::filesystem::path(s_scene_drop_path).stem().string().c_str());
        ImGui::Separator();


        if (ImGui::MenuItem("Scene (replace current)")) {
            Entity old_active = edit_state ? edit_state->active_scene : Entity{};
            if (world.is_alive(old_active))
                world.despawn(old_active);

            if (auto* ser = world.try_resource<SceneSerializer>()) {
                Entity root = ser->load_scene(world, s_scene_drop_path);
                if (edit_state) edit_state->active_scene = root;
            }
            resolve_mesh_paths(world);
            s_scene_drop_path.clear();
        }

        if (ImGui::MenuItem("Sub-scene (add under current)")) {
            Entity active = edit_state ? edit_state->active_scene : Entity{};
            if (auto* ser = world.try_resource<SceneSerializer>()) {
                Entity root = ser->load_scene(world, s_scene_drop_path);
                // Parent the loaded SceneRoot under the active scene
                if (world.is_alive(root) && world.is_alive(active))
                    set_parent(world, root, active);
            }
            resolve_mesh_paths(world);
            s_scene_drop_path.clear();
        }

        ImGui::EndPopup();
    }

    ImGui::End();
    ImGui::PopStyleVar();
}

} // namespace helios::editor
