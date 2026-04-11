#pragma once

#include <helios/ecs/entity.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_set>

namespace helios::editor {

enum class EditorMode : uint8_t {
    Edit,
    Play,
};

enum class GizmoOperation : uint8_t {
    Translate,
    Rotate,
    Scale,
};

enum class GizmoSpace : uint8_t {
    Local,
    World,
};

struct EditorState {
    // Selection
    Entity selected_entity{};

    // Mode
    EditorMode mode = EditorMode::Edit;

    // Gizmo
    GizmoOperation gizmo_op = GizmoOperation::Translate;
    GizmoSpace gizmo_space = GizmoSpace::Local;
    bool gizmo_snap = false;
    float translate_snap = 0.5f;
    float rotate_snap = 45.0f;
    float scale_snap = 0.1f;

    // Project
    std::filesystem::path project_path;
    std::string project_name;

    // Viewport
    bool viewport_focused = false;
    bool viewport_hovered = false;
    uint32_t viewport_width = 0;
    uint32_t viewport_height = 0;

    // Scene snapshot for Play/Stop restore (YAML string)
    std::string scene_snapshot;


    // Camera preview render target config
    uint32_t camera_preview_target_id = 9999;   // RenderContext camera_targets key
    uint32_t camera_preview_width = 320;
    uint32_t camera_preview_height = 180;

    // Panel visibility
    bool show_scene_hierarchy = true;
    bool show_inspector = true;
    bool show_content_browser = true;
    bool show_viewport = true;
    bool show_project_settings = false;  // modal, not always visible
    bool show_scene_settings = true;
    bool show_stats = true;
    bool show_console = true;
};

/// Editor-only scene editing context. Tracks which scenes are loaded,
/// which is active, and which have unsaved changes.
struct SceneEditState {
    Entity active_scene{};                     // the SceneRoot entity being edited
    std::unordered_set<uint32_t> dirty_scenes; // entity indices of SceneRoot entities with unsaved changes

    void mark_dirty(Entity scene_root) {
        dirty_scenes.insert(scene_root.index);
    }

    bool is_dirty(Entity scene_root) const {
        return dirty_scenes.count(scene_root.index) > 0;
    }

    void clear_dirty(Entity scene_root) {
        dirty_scenes.erase(scene_root.index);
    }
};

} // namespace helios::editor
