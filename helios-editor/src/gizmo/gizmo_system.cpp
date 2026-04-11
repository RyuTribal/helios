#include "gizmo_system.h"
#include "../editor_state.h"

#include <helios/ecs/world.h>
#include <helios/components/components.h>

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>
#include <ImGuizmo.h>

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace helios::editor {

void gizmo_system(World& world) {
    auto* state = world.try_resource<EditorState>();
    if (!state) return;
    if (state->mode != EditorMode::Edit) return;

    Entity entity = state->selected_entity;
    if (!world.is_alive(entity)) return;

    auto* transform = world.try_get<Transform>(entity);
    if (!transform) return;

    // Find editor camera view/projection
    glm::mat4 view{1.0f};
    glm::mat4 proj{1.0f};

    auto cam_q = world.query<const Transform, const Camera, With<EditorOnly>>();
    for (auto [cam_entity, cam_t, cam] : cam_q.with_entity()) {
        view = glm::inverse(cam_t.to_mat4());
        float aspect = (state->viewport_height > 0)
            ? static_cast<float>(state->viewport_width) / static_cast<float>(state->viewport_height)
            : 16.0f / 9.0f;
        proj = glm::perspective(glm::radians(cam.fov_degrees), aspect,
                                 cam.near_plane, cam.far_plane);
        break;
    }

    // Caller (viewport_panel) sets ImGuizmo::SetRect to the image rect.
    // We just configure draw list and projection mode.
    ImGuizmo::SetOrthographic(false);
    ImGuizmo::SetDrawlist();

    // Map gizmo operation
    ImGuizmo::OPERATION op = ImGuizmo::TRANSLATE;
    switch (state->gizmo_op) {
        case GizmoOperation::Translate: op = ImGuizmo::TRANSLATE; break;
        case GizmoOperation::Rotate:    op = ImGuizmo::ROTATE;    break;
        case GizmoOperation::Scale:     op = ImGuizmo::SCALE;     break;
    }

    ImGuizmo::MODE mode = (state->gizmo_space == GizmoSpace::Local)
        ? ImGuizmo::LOCAL : ImGuizmo::WORLD;

    // Snap values
    float snap_values[3] = {0, 0, 0};
    float* snap_ptr = nullptr;
    if (state->gizmo_snap) {
        float snap_val = state->translate_snap;
        if (state->gizmo_op == GizmoOperation::Rotate) snap_val = state->rotate_snap;
        if (state->gizmo_op == GizmoOperation::Scale)  snap_val = state->scale_snap;
        snap_values[0] = snap_values[1] = snap_values[2] = snap_val;
        snap_ptr = snap_values;
    }

    glm::mat4 model = transform->to_mat4();

    ImGuizmo::Manipulate(
        glm::value_ptr(view),
        glm::value_ptr(proj),
        op, mode,
        glm::value_ptr(model),
        nullptr, snap_ptr);

    if (ImGuizmo::IsUsing()) {
        // Decompose the manipulated matrix back into transform
        glm::vec3 translation, scale, skew;
        glm::vec4 perspective;
        glm::quat rotation;

        // Manual decomposition (glm::decompose is in gtx/matrix_decompose)
        translation = glm::vec3(model[3]);
        scale.x = glm::length(glm::vec3(model[0]));
        scale.y = glm::length(glm::vec3(model[1]));
        scale.z = glm::length(glm::vec3(model[2]));

        glm::mat3 rot_mat{
            glm::vec3(model[0]) / scale.x,
            glm::vec3(model[1]) / scale.y,
            glm::vec3(model[2]) / scale.z
        };
        rotation = glm::quat_cast(rot_mat);

        transform->position = translation;
        transform->rotation = rotation;
        transform->scale = scale;
    }
}

} // namespace helios::editor
