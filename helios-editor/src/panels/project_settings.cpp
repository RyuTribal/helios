#include "project_settings.h"
#include "../editor_state.h"
#include "../project/project.h"
#include "../icons/IconsMaterialDesignIcons.h"

#include <helios/ecs/world.h>
#include <helios/render_settings.h>

#include <imgui.h>

namespace helios::editor {

static const char* present_mode_names[] = {"Immediate", "Fifo", "Mailbox"};

static int present_mode_to_index(rhi::PresentMode mode) {
    switch (mode) {
        case rhi::PresentMode::Immediate: return 0;
        case rhi::PresentMode::Fifo:      return 1;
        case rhi::PresentMode::Mailbox:   return 2;
        default: return 1;
    }
}

static rhi::PresentMode index_to_present_mode(int idx) {
    switch (idx) {
        case 0: return rhi::PresentMode::Immediate;
        case 1: return rhi::PresentMode::Fifo;
        case 2: return rhi::PresentMode::Mailbox;
        default: return rhi::PresentMode::Fifo;
    }
}

void project_settings_panel(World& world) {
    auto* state = world.try_resource<EditorState>();
    if (!state) return;

    if (state->show_project_settings) {
        ImGui::OpenPopup("Project Settings");
        state->show_project_settings = false;
    }

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_Appearing);

    if (!ImGui::BeginPopupModal("Project Settings", nullptr, ImGuiWindowFlags_NoResize)) return;

    auto* project = world.try_resource<Project>();
    auto* settings = world.try_resource<RenderSettings>();

    if (project && !project->name.empty()) {
        ImGui::Text(ICON_MDI_BRIEFCASE " Project: %s", project->name.c_str());
        ImGui::Text("Directory: %s", project->project_dir.string().c_str());
        ImGui::Separator();

        char script_buf[512];
        strncpy(script_buf, project->script_assembly.c_str(), sizeof(script_buf) - 1);
        script_buf[sizeof(script_buf) - 1] = '\0';
        if (ImGui::InputText(ICON_MDI_LANGUAGE_CSHARP " Script Assembly", script_buf, sizeof(script_buf))) {
            project->script_assembly = script_buf;
        }

        ImGui::Text("Asset Registry: %zu entries", project->registry.entries.size());
        ImGui::Separator();
    } else {
        ImGui::Text("No project loaded");
        ImGui::Separator();
    }

    ImGui::Text(ICON_MDI_MONITOR " Renderer");

    if (settings) {
        // Present mode dropdown
        int present_idx = present_mode_to_index(settings->present_mode);
        if (ImGui::Combo("Present Mode", &present_idx, present_mode_names, 3)) {
            settings->set_present_mode(index_to_present_mode(present_idx));
        }

        if (project) {
            const char* aa_types[] = {"None", "MSAA", "SSAA"};
            int aa_idx = 0;
            if (project->renderer.anti_aliasing == "MSAA") aa_idx = 1;
            if (project->renderer.anti_aliasing == "SSAA") aa_idx = 2;
            if (ImGui::Combo("Anti-Aliasing", &aa_idx, aa_types, 3)) {
                project->renderer.anti_aliasing = aa_types[aa_idx];
            }
            ImGui::DragInt("AA Multiplier", &project->renderer.multiplier, 1, 1, 8);
        }

        ImGui::Separator();

        if (ImGui::Button(ICON_MDI_CONTENT_SAVE " Apply", ImVec2(120, 0))) {
            // Persist to project
            if (project) {
                int cur_idx = present_mode_to_index(settings->present_mode);
                project->renderer.present_mode = present_mode_names[cur_idx];
                project->save();
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Close", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
    } else {
        ImGui::Text("RenderSettings not available");
    }

    ImGui::EndPopup();
}

} // namespace helios::editor
