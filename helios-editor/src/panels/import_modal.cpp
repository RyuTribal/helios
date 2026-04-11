#include "import_modal.h"
#include "../project/project.h"
#include "../icons/IconsMaterialDesignIcons.h"

#include <helios/assets/asset_server.h>
#include <helios/core/log_macros.h>

#include <imgui.h>
#include <nfd.h>

#include <algorithm>
#include <filesystem>
#include <memory>

HELIOS_DECLARE_LOG_CHANNEL(Editor);

namespace helios::editor {

// Human-readable names for AssetBinaryType, indexed by enum value.
static const char* s_type_names[] = {
    "Unknown",   // 0
    "Mesh",      // 1
    "Texture",   // 2
    "CubeMap",   // 3
    "Audio",     // 4
    "Shader",    // 5
    "Material",  // 6
    "Scene",     // 7
};
static constexpr int s_type_count = static_cast<int>(std::size(s_type_names));

static int type_to_index(AssetBinaryType type) {
    int v = static_cast<int>(type);
    if (v >= 0 && v < s_type_count) return v;
    return 0; // Unknown
}

static AssetBinaryType index_to_type(int idx) {
    if (idx >= 0 && idx < s_type_count) return static_cast<AssetBinaryType>(idx);
    return AssetBinaryType::Unknown;
}

// Suggest an output filename: take the source filename stem and add a
// Helios extension based on the detected type.
static std::string suggest_dest_filename(const std::string& source_path,
                                         AssetBinaryType type) {
    auto stem = std::filesystem::path(source_path).stem().string();
    switch (type) {
        case AssetBinaryType::Mesh:     return stem + ".hvemesh";
        case AssetBinaryType::Texture:  return stem + ".hvetex";
        case AssetBinaryType::CubeMap:  return stem + ".hvecube";
        case AssetBinaryType::Audio:    return stem + ".hveaudio";
        case AssetBinaryType::Shader:   return stem + ".hveshader";
        case AssetBinaryType::Material: return stem + ".hvemat";
        case AssetBinaryType::Scene:    return stem + ".hvescn";
        default:                        return stem + ".hlasset";
    }
}

void import_modal(World& world, ImportModalState& state) {
    if (state.open) {
        ImGui::OpenPopup("Import Asset");
        state.open = false;
    }

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(550, 400), ImGuiCond_Appearing);

    if (!ImGui::BeginPopupModal("Import Asset", nullptr, ImGuiWindowFlags_None))
        return;

    // --- Source path (read-only) ---
    ImGui::Text(ICON_MDI_FILE " Source:");
    ImGui::SameLine();
    ImGui::TextWrapped("%s", state.source_path.c_str());
    ImGui::Separator();

    // --- Detected type (combo dropdown) ---
    int type_idx = type_to_index(state.detected_type);
    if (ImGui::Combo("Asset Type", &type_idx, s_type_names, s_type_count)) {
        state.detected_type = index_to_type(type_idx);
        // Re-suggest filename when type changes
        state.dest_filename = suggest_dest_filename(state.source_path,
                                                    state.detected_type);
    }

    ImGui::Separator();

    // --- Metadata key-value entries ---
    ImGui::Text(ICON_MDI_TAG " Metadata");

    // We must iterate safely because we may erase entries.
    // Collect keys first, then iterate copies.
    std::vector<std::string> keys;
    keys.reserve(state.metadata.size());
    for (const auto& [k, v] : state.metadata) {
        keys.push_back(k);
    }
    std::sort(keys.begin(), keys.end());

    std::string to_erase;
    for (auto& key : keys) {
        // Skip system-internal fields (start with '_')
        if (!key.empty() && key[0] == '_') continue;

        ImGui::PushID(key.c_str());

        char key_buf[256];
        strncpy(key_buf, key.c_str(), sizeof(key_buf) - 1);
        key_buf[sizeof(key_buf) - 1] = '\0';

        char val_buf[512];
        strncpy(val_buf, state.metadata[key].c_str(), sizeof(val_buf) - 1);
        val_buf[sizeof(val_buf) - 1] = '\0';

        ImGui::SetNextItemWidth(150);
        ImGui::InputText("##key", key_buf, sizeof(key_buf),
                         ImGuiInputTextFlags_ReadOnly);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-60);
        if (ImGui::InputText("##val", val_buf, sizeof(val_buf))) {
            state.metadata[key] = val_buf;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton(ICON_MDI_DELETE)) {
            to_erase = key;
        }

        ImGui::PopID();
    }

    if (!to_erase.empty()) {
        state.metadata.erase(to_erase);
    }

    if (ImGui::Button(ICON_MDI_PLUS " Add Field")) {
        // Find a unique key name
        int n = 0;
        std::string new_key = "custom_field";
        while (state.metadata.count(new_key) > 0) {
            new_key = "custom_field_" + std::to_string(++n);
        }
        state.metadata[new_key] = "";
    }

    ImGui::Separator();

    // --- Destination filename ---
    char dest_buf[512];
    strncpy(dest_buf, state.dest_filename.c_str(), sizeof(dest_buf) - 1);
    dest_buf[sizeof(dest_buf) - 1] = '\0';
    if (ImGui::InputText("Output Filename", dest_buf, sizeof(dest_buf))) {
        state.dest_filename = dest_buf;
    }

    // --- Destination directory override ---
    std::string dir_display = state.dest_dir_override.empty()
        ? "(auto - same as source)" : state.dest_dir_override;
    ImGui::Text("Output Dir: %s", dir_display.c_str());
    ImGui::SameLine();
    if (ImGui::Button("...##dest_dir")) {
        nfdu8char_t* out_path = nullptr;
        if (NFD_PickFolder(&out_path, nullptr) == NFD_OKAY) {
            state.dest_dir_override = out_path;
            NFD_FreePath(out_path);
        }
    }
    if (!state.dest_dir_override.empty()) {
        ImGui::SameLine();
        if (ImGui::SmallButton("Reset##dest_dir_reset")) {
            state.dest_dir_override.clear();
        }
    }

    ImGui::Separator();

    // --- Import / Cancel buttons ---
    bool has_server = world.has_resource<std::shared_ptr<AssetServer>>();
    if (!has_server) {
        ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1),
                           "No AssetServer available");
    }

    // Disable import if no server or type is Unknown
    bool can_import = has_server &&
                      state.detected_type != AssetBinaryType::Unknown &&
                      !state.dest_filename.empty();

    if (!can_import) ImGui::BeginDisabled();
    if (ImGui::Button(ICON_MDI_IMPORT " Import", ImVec2(120, 0))) {
        auto& server = world.resource<std::shared_ptr<AssetServer>>();

        // Compute dest_relative_path: relative to asset root.
        // The asset root is server->root(). The dest_filename is just
        // the filename; place it relative to where the source file lives
        // within the asset tree, or at the root if outside.
        std::filesystem::path source(state.source_path);
        std::filesystem::path asset_root = server->root();
        std::filesystem::path dest_dir;

        if (!state.dest_dir_override.empty()) {
            // User chose a custom output directory
            dest_dir = std::filesystem::relative(state.dest_dir_override, asset_root);
        } else {
            // If the source file is inside the asset root, keep its
            // directory structure. Otherwise place at asset root.
            auto rel = std::filesystem::relative(source.parent_path(), asset_root);
            if (!rel.empty() && rel.string().find("..") != 0) {
                dest_dir = rel;
            }
        }

        std::string dest_relative =
            (dest_dir / state.dest_filename).string();

        auto result = server->import_asset(state.source_path,
                                 state.detected_type,
                                 dest_relative,
                                 state.metadata);
        if (!result.empty()) {
            HELIOS_LOG(Editor, Info, "Imported '{}' -> '{}'",
                       state.source_path, dest_relative);
        } else {
            HELIOS_LOG(Editor, Error, "Import failed for '{}'", state.source_path);
        }

        ImGui::CloseCurrentPopup();
    }
    if (!can_import) ImGui::EndDisabled();

    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(120, 0))) {
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}

} // namespace helios::editor
