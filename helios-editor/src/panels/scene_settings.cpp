#include "scene_settings.h"
#include "../editor_state.h"
#include "../icons/IconsMaterialDesignIcons.h"

#include <helios/ecs/world.h>
#include <helios/forward_plus/forward_plus_config.h>
#include <helios/render_settings.h>

#include "../editor_utils.h"

#include <imgui.h>
#include <glm/gtc/type_ptr.hpp>
#include <filesystem>
#include <string>

namespace helios::editor {

static constexpr float kSceneSettingsLabelWidth = 130.0f;

void scene_settings_panel(World& world) {
    auto* state = world.try_resource<EditorState>();
    if (!state || !state->show_scene_settings) return;

    ImGui::Begin("Scene Settings");

    // Skybox (ForwardPlusConfig)
    auto* config = world.try_resource<ForwardPlusConfig>();
    if (config) {
        ImGui::Text(ICON_MDI_IMAGE_FILTER_HDR " Skybox");

        ImGui::Text("Path: %s", config->skybox_hdr_path.empty()
            ? "(none)" : config->skybox_hdr_path.c_str());

        char buf[512];
        strncpy(buf, config->skybox_hdr_path.c_str(), sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        if (ImGui::InputText("##SkyboxHDR", buf, sizeof(buf))) {
            config->skybox_hdr_path = buf;
        }

        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
                std::string path(static_cast<const char*>(payload->Data));
                std::filesystem::path p(path);
                auto ext = p.extension().string();
                if (ext == ".hdr" || ext == ".dds" || ext == ".exr") {
                    config->skybox_hdr_path = path;
                }
            }
            ImGui::EndDragDropTarget();
        }

        ImGui::Separator();
    }

    // Render settings
    auto* settings = world.try_resource<RenderSettings>();
    if (settings) {
        ImGui::Text(ICON_MDI_TUNE " Rendering");

        // Clear color
        glm::vec3 cc(settings->clear_color);
        begin_field("Clear Color", kSceneSettingsLabelWidth);
        ImGui::SetNextItemWidth(-1);
        if (ImGui::ColorEdit3("##clear", glm::value_ptr(cc))) {
            settings->clear_color = glm::vec4(cc, 1.0f);
        }
        end_field();

        begin_field("Exposure", kSceneSettingsLabelWidth);
        ImGui::SetNextItemWidth(-1);
        ImGui::DragFloat("##exposure", &settings->exposure, 0.01f, 0.0f, 20.0f);
        end_field();

        begin_field("Ambient Color", kSceneSettingsLabelWidth);
        ImGui::SetNextItemWidth(-1);
        ImGui::ColorEdit3("##ambient_color", glm::value_ptr(settings->ambient_color));
        end_field();

        begin_field("Ambient Intensity", kSceneSettingsLabelWidth);
        ImGui::SetNextItemWidth(-1);
        ImGui::DragFloat("##ambient_intensity", &settings->ambient_intensity, 0.01f, 0.0f, 5.0f);
        end_field();

        begin_field("Resolution Scale", kSceneSettingsLabelWidth);
        ImGui::SetNextItemWidth(-1);
        ImGui::DragFloat("##res_scale", &settings->resolution_scale, 0.01f, 0.25f, 2.0f);
        end_field();

        // Tonemapping
        const char* tonemap_names[] = {"None", "Reinhard", "ACES", "Filmic"};
        int tonemap_idx = static_cast<int>(settings->tonemap);
        begin_field("Tonemapping", kSceneSettingsLabelWidth);
        ImGui::SetNextItemWidth(-1);
        if (ImGui::Combo("##tonemap", &tonemap_idx, tonemap_names, 4)) {
            settings->tonemap = static_cast<TonemapMode>(tonemap_idx);
        }
        end_field();

        // Quality
        ImGui::Separator();
        ImGui::Text(ICON_MDI_SPEEDOMETER " Quality");

        int shadow_res = static_cast<int>(settings->shadow_resolution);
        begin_field("Shadow Resolution", kSceneSettingsLabelWidth);
        ImGui::SetNextItemWidth(-1);
        if (ImGui::DragInt("##shadow_res", &shadow_res, 64, 256, 8192)) {
            settings->set_shadow_resolution(static_cast<uint32_t>(shadow_res));
        }
        end_field();

        int cascades = static_cast<int>(settings->shadow_cascades);
        begin_field("Shadow Cascades", kSceneSettingsLabelWidth);
        ImGui::SetNextItemWidth(-1);
        if (ImGui::DragInt("##shadow_cascades", &cascades, 1, 1, 8)) {
            settings->shadow_cascades = static_cast<uint32_t>(cascades);
        }
        end_field();

        begin_field("LOD Bias", kSceneSettingsLabelWidth);
        ImGui::SetNextItemWidth(-1);
        ImGui::DragFloat("##lod_bias", &settings->lod_bias, 0.1f, 0.1f, 10.0f);
        end_field();

        // Debug (bitwise flags)
        ImGui::Separator();
        ImGui::Text(ICON_MDI_BUG " Debug");

        auto debug_toggle = [&](const char* label, DebugDraw flag) {
            bool on = settings->has_debug(flag);
            ImGui::PushID(label);
            begin_field(label, kSceneSettingsLabelWidth);
            if (ImGui::Checkbox("##v", &on)) settings->toggle_debug(flag);
            end_field();
            ImGui::PopID();
        };
        debug_toggle("Wireframe", DebugDraw::Wireframe);
        debug_toggle("Show Colliders", DebugDraw::Colliders);
        debug_toggle("Show Light Bounds", DebugDraw::LightBounds);
        debug_toggle("Show Normals", DebugDraw::Normals);
        debug_toggle("Bounding Boxes", DebugDraw::BoundingBoxes);
    }

    ImGui::End();
}

} // namespace helios::editor
