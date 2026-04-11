#include "editor_camera.h"
#include "editor_state.h"

#include <helios/ecs/world.h>
#include <helios/components/components.h>
#include <helios/input/raw_input.h>
#include <helios/window/windows.h>
#include <helios/ecs/time.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <imgui.h>

#include <cmath>

namespace helios::editor {

void setup_editor_camera(World& world) {
    world.insert_resource(EditorCameraController{});

    world.spawn(
        Transform{ .position = glm::vec3{0.0f, 2.0f, 8.0f} },
        Camera{
            .fov_degrees = 60.0f,
            .near_plane = 0.1f,
            .far_plane = 1000.0f,
            .order = 0,
        },
        ActiveCamera{},
        EditorOnly{},
        Tag{ .name = "EditorCamera" }
    );
}

void update_editor_camera(World& world) {
    auto* state = world.try_resource<EditorState>();
    auto* ctrl = world.try_resource<EditorCameraController>();
    auto* input = world.try_resource<RawInput>();
    auto* windows = world.try_resource<Windows>();
    if (!state || !ctrl || !input) return;

    // Cursor lock management — must run EVERY frame regardless of hover,
    // otherwise releasing RMB while cursor is outside viewport leaves it stuck.
    bool rmb_now = input->mouse_button_pressed(MouseButton::Right);

    if (windows && windows->has_primary()) {
        // Start capture: only when viewport is hovered AND in edit mode
        if (rmb_now && !ctrl->cursor_captured && state->viewport_hovered && state->mode == EditorMode::Edit) {
            windows->primary().set_cursor_mode(Window::CursorMode::Captured);
            ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouse;
            ctrl->cursor_captured = true;
        }
        // End capture: always, regardless of hover state
        if (!rmb_now && ctrl->cursor_captured) {
            windows->primary().set_cursor_mode(Window::CursorMode::Normal);
            ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
            ctrl->cursor_captured = false;
        }
    }

    // Only process camera movement when captured or viewport is hovered
    if (!ctrl->cursor_captured && (!state->viewport_hovered || state->mode != EditorMode::Edit)) return;

    auto delta = input->mouse_delta();
    float scroll = input->scroll_delta();
    bool mmb = input->mouse_button_pressed(MouseButton::Middle);
    bool shift = input->key_pressed(KeyCode::LeftShift) ||
                 input->key_pressed(KeyCode::RightShift);
    bool alt = input->key_pressed(KeyCode::LeftAlt) ||
               input->key_pressed(KeyCode::RightAlt);

    // Orbit: Alt + MMB or RMB drag
    bool rmb = input->mouse_button_pressed(MouseButton::Right);
    if ((alt && mmb) || rmb) {
        ctrl->yaw += delta.x * ctrl->rotate_speed;
        ctrl->pitch -= delta.y * ctrl->rotate_speed;
        ctrl->pitch = glm::clamp(ctrl->pitch, -glm::half_pi<float>() + 0.02f,
                                                glm::half_pi<float>() - 0.02f);
    }

    // Pan: Shift + MMB or MMB drag
    if (mmb && shift) {
        float pan_scale = ctrl->distance * ctrl->pan_speed;
        // Compute right and up vectors from yaw/pitch
        glm::vec3 forward{
            std::cos(ctrl->pitch) * std::sin(ctrl->yaw),
            std::sin(ctrl->pitch),
            std::cos(ctrl->pitch) * std::cos(ctrl->yaw)
        };
        glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0, 1, 0)));
        glm::vec3 up = glm::normalize(glm::cross(right, forward));

        ctrl->focal_point -= right * delta.x * pan_scale;
        ctrl->focal_point -= up * delta.y * pan_scale;
    }

    // Zoom: scroll wheel
    if (scroll != 0.0f) {
        ctrl->distance *= 1.0f - scroll * ctrl->zoom_speed;
        ctrl->distance = glm::clamp(ctrl->distance, ctrl->min_distance, ctrl->max_distance);
    }

    // Compute camera position from spherical coordinates around focal point
    float cos_pitch = std::cos(ctrl->pitch);
    glm::vec3 offset{
        ctrl->distance * cos_pitch * std::sin(ctrl->yaw),
        ctrl->distance * std::sin(ctrl->pitch),
        ctrl->distance * cos_pitch * std::cos(ctrl->yaw)
    };

    glm::vec3 cam_pos = ctrl->focal_point + offset;

    // Apply to the editor camera entity
    auto q = world.query<Transform, const Camera, With<EditorOnly>>();
    for (auto [entity, transform, cam] : q.with_entity()) {
        {
            transform.position = cam_pos;
            glm::mat4 look = glm::lookAt(cam_pos, ctrl->focal_point, glm::vec3(0, 1, 0));
            transform.rotation = glm::conjugate(glm::quat_cast(look));
            break;
        }
    }
}

} // namespace helios::editor
