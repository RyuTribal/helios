#include "scene_hierarchy.h"
#include "../editor_state.h"
#include "../editor_commands.h"

#include <helios/ecs/world.h>
#include <helios/ecs/hierarchy.h>
#include <helios/components/components.h>
#include <helios/serialization/scene_serializer.h>

#include <imgui.h>
#include <nfd.h>
#include "../icons/IconsMaterialDesignIcons.h"
#include "../project/project.h"

namespace helios::editor {

static void display_entity_node(Entity entity, World& world, EditorState& state,
                                EditorCommands& commands);

void scene_hierarchy_panel(World& world) {
    auto* state = world.try_resource<EditorState>();
    if (!state || !state->show_scene_hierarchy) return;

    auto* commands = world.try_resource<EditorCommands>();
    if (!commands) return;

    ImGui::Begin("Scene Hierarchy");

    // Iterate root entities (those without Parent), skip editor-internal entities
    auto root_query = world.query<const Tag, Without<Parent>>();
    for (auto [entity, tag] : root_query.with_entity()) {
        if (world.has<EditorOnly>(entity)) continue;
        display_entity_node(entity, world, *state, *commands);
    }

    // Drop on empty space = reparent to root
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SCENE_NODE")) {
            Entity dragged = *static_cast<const Entity*>(payload->Data);
            unparent(world, dragged);
        }
        ImGui::EndDragDropTarget();
    }

    // Deselect on empty space click
    if (ImGui::IsMouseDown(0) && ImGui::IsWindowHovered()) {
        state->selected_entity = Entity{};
    }

    // Right-click context menu on empty space
    if (ImGui::BeginPopupContextWindow(nullptr,
            ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
        if (ImGui::MenuItem(ICON_MDI_PLUS " Create Empty Entity")) {
            auto e = world.spawn(Tag{.name = "Empty Entity"}, Transform{});

            auto* edit_state = world.try_resource<SceneEditState>();
            if (edit_state && world.is_alive(edit_state->active_scene))
                set_parent(world, e, edit_state->active_scene);

            state->selected_entity = e;
        }
        ImGui::EndPopup();
    }

    ImGui::End();
}

static void display_entity_node(Entity entity, World& world, EditorState& state,
                                EditorCommands& commands) {
    auto* tag = world.try_get<Tag>(entity);
    std::string name = tag ? tag->name : ("Entity " + std::to_string(entity.index));

    const char* icon = ICON_MDI_CUBE_OUTLINE;
    bool is_scene_root = world.has<SceneRoot>(entity);
    if (is_scene_root)                              icon = ICON_MDI_FILMSTRIP;
    else if (world.has<Camera>(entity))             icon = ICON_MDI_VIDEO;
    else if (world.has<PointLight>(entity))         icon = ICON_MDI_LIGHTBULB;
    else if (world.has<DirectionalLight>(entity))   icon = ICON_MDI_WHITE_BALANCE_SUNNY;
    else if (world.has<AudioSource>(entity))        icon = ICON_MDI_MUSIC_NOTE;

    std::string label = std::string(icon) + " " + name;

    auto* edit_state = world.try_resource<SceneEditState>();
    if (is_scene_root && edit_state) {
        if (entity == edit_state->active_scene) label += " [active]";
        if (edit_state->is_dirty(entity)) label += " *";
    }

    ImGui::PushID(static_cast<int>(entity.index));

    auto* children = world.try_get<Children>(entity);
    bool has_children = children && !children->entities.empty();

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (!has_children) flags |= ImGuiTreeNodeFlags_Leaf;
    if (entity == state.selected_entity) flags |= ImGuiTreeNodeFlags_Selected;

    bool opened = ImGui::TreeNodeEx(label.c_str(), flags);

    // Click to select
    if (ImGui::IsItemClicked()) {
        state.selected_entity = entity;
    }

    // Drag source
    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
        ImGui::SetDragDropPayload("SCENE_NODE", &entity, sizeof(Entity));
        ImGui::Text("%s %s", icon, name.c_str());
        ImGui::EndDragDropSource();
    }

    // Drop target — reparent dragged entity under this one
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SCENE_NODE")) {
            Entity dragged = *static_cast<const Entity*>(payload->Data);
            if (dragged != entity)
                set_parent(world, dragged, entity);
        }
        ImGui::EndDragDropTarget();
    }

    // Right-click context menu
    if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem(ICON_MDI_PLUS " Create Child Entity")) {
            auto child = world.spawn(Tag{.name = "Child"}, Transform{});
            set_parent(world, child, entity);
            state.selected_entity = child;
        }
        if (ImGui::MenuItem(ICON_MDI_CONTENT_SAVE " Save As Scene...")) {
            nfdu8filteritem_t filters[] = {{ "Helios Scene", "hvescn" }};
            nfdu8char_t* out_path = nullptr;
            auto* tag_ptr = world.try_get<Tag>(entity);
            std::string default_name = tag_ptr ? tag_ptr->name + ".hvescn" : "scene.hvescn";
            if (NFD_SaveDialog(&out_path, filters, 1, nullptr,
                               default_name.c_str()) == NFD_OKAY) {
                // Add SceneRoot BEFORE saving so it's included in the file
                if (!world.has<SceneRoot>(entity)) {
                    std::string scene_name = tag_ptr ? tag_ptr->name : "Untitled";
                    std::string rel_path;
                    if (auto* proj = world.try_resource<Project>()) {
                        rel_path = std::filesystem::relative(
                            out_path, proj->asset_dir()).string();
                    } else {
                        rel_path = out_path;
                    }
                    world.add(entity, SceneRoot{
                        .scene_name = scene_name,
                        .scene_path = rel_path,
                    });
                }
                if (auto* ser = world.try_resource<SceneSerializer>()) {
                    ser->save_entities(world, {entity}, out_path);
                }
                NFD_FreePath(out_path);
            }
        }
        ImGui::Separator();
        if (ImGui::MenuItem(ICON_MDI_DELETE " Delete Entity")) {
            if (state.selected_entity == entity) {
                state.selected_entity = Entity{};
            }
            world.despawn(entity);  // handles children + parent cleanup automatically
        }
        ImGui::EndPopup();
    }

    if (opened) {
        if (has_children) {
            for (auto child : children->entities) {
                if (!world.is_alive(child)) continue;  // skip dead references
                display_entity_node(child, world, state, commands);
            }
        }
        ImGui::TreePop();
    }

    ImGui::PopID();
}

} // namespace helios::editor
