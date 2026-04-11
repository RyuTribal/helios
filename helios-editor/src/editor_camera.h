#pragma once

#include <helios/ecs/world.h>
#include <glm/glm.hpp>

namespace helios::editor {

/// Resource tracking editor camera orbit state.
struct EditorCameraController {
    float yaw = 0.0f;
    float pitch = 0.0f;
    float distance = 8.0f;
    float pan_speed = 0.01f;
    float rotate_speed = 0.003f;
    float zoom_speed = 0.3f;
    float min_distance = 0.5f;
    float max_distance = 200.0f;

    glm::vec3 focal_point{0.0f};
    bool cursor_captured = false;
};

void setup_editor_camera(World& world);
void update_editor_camera(World& world);

} // namespace helios::editor
