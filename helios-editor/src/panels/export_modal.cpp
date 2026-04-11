#include "export_modal.h"
#include "../icons/IconsMaterialDesignIcons.h"

#include <helios/assets/asset_server.h>
#include <helios/core/log_macros.h>

#include <imgui.h>
#include <nfd.h>

#include <filesystem>
#include <fstream>
#include <memory>

HELIOS_DECLARE_LOG_CHANNEL(Editor);

namespace helios::editor {

void export_modal(World& world, ExportModalState& state) {
    if (state.open) {
        ImGui::OpenPopup("Export Asset");
        state.open = false;
    }

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(450, 250), ImGuiCond_Appearing);

    if (!ImGui::BeginPopupModal("Export Asset", nullptr, ImGuiWindowFlags_None))
        return;

    // --- Asset path (read-only) ---
    ImGui::Text(ICON_MDI_FILE " Asset:");
    ImGui::SameLine();
    ImGui::TextWrapped("%s", state.asset_path.c_str());
    ImGui::Separator();

    // --- Format dropdown ---
    if (state.formats.empty()) {
        ImGui::TextColored(ImVec4(1, 0.7f, 0.3f, 1),
                           "No export formats registered for this asset type.");
    } else {
        // Build a combined string for ImGui::Combo
        if (ImGui::BeginCombo("Format",
                              state.selected_format >= 0 &&
                              state.selected_format < static_cast<int>(state.formats.size())
                                  ? state.formats[state.selected_format].c_str()
                                  : "")) {
            for (int i = 0; i < static_cast<int>(state.formats.size()); ++i) {
                bool selected = (i == state.selected_format);
                if (ImGui::Selectable(state.formats[i].c_str(), selected)) {
                    state.selected_format = i;
                }
                if (selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
    }

    ImGui::Separator();

    // --- Export / Cancel buttons ---
    bool has_server = world.has_resource<std::shared_ptr<AssetServer>>();
    bool can_export = has_server && !state.formats.empty() &&
                      state.selected_format >= 0 &&
                      state.selected_format < static_cast<int>(state.formats.size());

    if (!can_export) ImGui::BeginDisabled();
    if (ImGui::Button(ICON_MDI_EXPORT " Export...", ImVec2(140, 0))) {
        // Open a save file dialog
        const auto& format = state.formats[state.selected_format];

        nfdu8filteritem_t filters[] = {
            { format.c_str(), format.c_str() }
        };

        auto stem = std::filesystem::path(state.asset_path).stem().string();
        std::string default_name = stem + "." + format;

        nfdu8char_t* out_path = nullptr;
        if (NFD_SaveDialog(&out_path, filters, 1, nullptr,
                           default_name.c_str()) == NFD_OKAY) {
            auto& server = world.resource<std::shared_ptr<AssetServer>>();
            auto bytes = server->export_asset(state.asset_path, format);

            if (!bytes.empty()) {
                // Write to the chosen path
                std::ofstream f(out_path, std::ios::binary);
                if (f.is_open()) {
                    f.write(reinterpret_cast<const char*>(bytes.data()),
                            static_cast<std::streamsize>(bytes.size()));
                    HELIOS_LOG(Editor, Info, "Exported '{}' -> '{}'",
                               state.asset_path, out_path);
                } else {
                    HELIOS_LOG(Editor, Error,
                               "Failed to write export file: {}", out_path);
                }
            } else {
                HELIOS_LOG(Editor, Error, "Export failed for '{}'", state.asset_path);
            }
            NFD_FreePath(out_path);
        }

        ImGui::CloseCurrentPopup();
    }
    if (!can_export) ImGui::EndDisabled();

    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(120, 0))) {
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}

} // namespace helios::editor
