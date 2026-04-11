#include "content_browser.h"
#include "import_modal.h"
#include "export_modal.h"
#include "../editor_state.h"
#include "../project/project.h"
#include "../icons/IconsMaterialDesignIcons.h"

#include <helios/ecs/world.h>
#include <helios/assets/asset_server.h>
#include <helios/assets/asset_binary.h>
#include <helios/core/log_macros.h>

#include <imgui.h>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_set>

HELIOS_DECLARE_LOG_CHANNEL(Editor);

namespace helios::editor {

// ---- Modal state (file-scoped) ----
static ImportModalState s_import_state;
static ExportModalState s_export_state;
static bool s_show_all_files = false;

// Helios file extensions shown by default
static const std::unordered_set<std::string> s_helios_exts = {
    ".hvemesh", ".hvetex", ".hvecube", ".hveaudio",
    ".hveshader", ".hvemat", ".hlasset", ".hvescn",
    ".hveproject", ".hvereg",
    ".cs", ".csproj",
};

// Directories always hidden
static const std::unordered_set<std::string> s_ignored_dirs = {
    "bin", "obj", ".vs", ".git", ".idea", "__pycache__",
    "node_modules", ".cache", "build",
};

static bool should_ignore(const std::filesystem::directory_entry& entry) {
    auto name = entry.path().filename().string();

    // Always hide dotfiles
    if (!name.empty() && name[0] == '.') return true;

    if (entry.is_directory()) {
        return s_ignored_dirs.count(name) > 0;
    }

    // Show All Files mode: show everything except junk dirs
    if (s_show_all_files) return false;

    // Default: only Helios files
    auto ext = entry.path().extension().string();
    return s_helios_exts.count(ext) == 0;
}

static const char* get_file_icon(const std::string& ext) {
    if (ext == ".gltf" || ext == ".glb" || ext == ".fbx" || ext == ".FBX" || ext == ".obj")
        return ICON_MDI_CUBE_OUTLINE;
    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg")
        return ICON_MDI_IMAGE;
    if (ext == ".hdr")
        return ICON_MDI_IMAGE_FILTER_HDR;
    if (ext == ".wav" || ext == ".ogg" || ext == ".mp3")
        return ICON_MDI_MUSIC_NOTE;
    if (ext == ".cs")
        return ICON_MDI_LANGUAGE_CSHARP;
    if (ext == ".hvescn")
        return ICON_MDI_FILMSTRIP;
    if (ext == ".hveproject")
        return ICON_MDI_BRIEFCASE;
    if (ext == ".csproj" || ext == ".sln")
        return ICON_MDI_CODE_BRACES;
    // Helios binary assets
    if (ext == ".hvemesh")
        return ICON_MDI_CUBE_OUTLINE;
    if (ext == ".hvetex")
        return ICON_MDI_IMAGE;
    if (ext == ".hvecube")
        return ICON_MDI_IMAGE_FILTER_HDR;
    if (ext == ".hveaudio")
        return ICON_MDI_MUSIC_NOTE;
    return ICON_MDI_FILE;
}

// Suggest a Helios binary extension for an asset type.
static std::string helios_ext_for_type(AssetBinaryType type) {
    switch (type) {
        case AssetBinaryType::Mesh:     return ".hvemesh";
        case AssetBinaryType::Texture:  return ".hvetex";
        case AssetBinaryType::CubeMap:  return ".hvecube";
        case AssetBinaryType::Audio:    return ".hveaudio";
        case AssetBinaryType::Shader:   return ".hveshader";
        case AssetBinaryType::Material: return ".hvemat";
        case AssetBinaryType::Scene:    return ".hvescn";
        default:                        return ".hlasset";
    }
}

// Open the import modal for a given source file.
static void open_import_for(const std::filesystem::path& path,
                            AssetServer* server) {
    s_import_state.open = true;
    s_import_state.source_path = path.string();
    s_import_state.metadata.clear();

    // Detect type from extension
    auto ext = path.extension().string();
    if (!ext.empty() && ext[0] == '.') ext = ext.substr(1);
    auto detected = server ? server->detect_import_type(ext) : std::nullopt;
    s_import_state.detected_type = detected.value_or(AssetBinaryType::Unknown);

    // Pre-fill default metadata from import settings
    if (server) {
        const auto* settings = server->get_import_settings(s_import_state.detected_type);
        if (settings) {
            s_import_state.metadata = settings->default_metadata;
        }
    }

    // Suggest output filename
    auto stem = path.stem().string();
    s_import_state.dest_filename =
        stem + helios_ext_for_type(s_import_state.detected_type);
}

// Open the export modal for a given Helios binary asset.
static void open_export_for(const std::filesystem::path& path,
                            AssetServer* server) {
    s_export_state.open = true;
    s_export_state.asset_path = path.string();
    s_export_state.formats.clear();
    s_export_state.selected_format = 0;

    // Populate available export formats from import settings.
    // We need to read the asset header to know its type.
    if (server) {
        // Read the header to find the type, then look up its settings.
        // We read the file directly for header inspection.
        std::ifstream f(path, std::ios::binary | std::ios::ate);
        if (f.is_open()) {
            auto size = f.tellg();
            f.seekg(0, std::ios::beg);
            std::vector<uint8_t> bytes(static_cast<size_t>(size));
            f.read(reinterpret_cast<char*>(bytes.data()), size);

            auto header = read_asset_header(bytes.data(), bytes.size());
            if (header) {
                const auto* settings = server->get_import_settings(header->type);
                if (settings) {
                    s_export_state.formats = settings->export_formats;
                }
            }
        }
    }
}

static void draw_directory_tree(const std::filesystem::path& dir,
                                AssetServer* server) {
    if (!std::filesystem::exists(dir) || !std::filesystem::is_directory(dir)) return;

    std::vector<std::filesystem::directory_entry> dirs, files;
    for (auto& entry : std::filesystem::directory_iterator(dir)) {
        if (should_ignore(entry)) continue;
        if (entry.is_directory()) dirs.push_back(entry);
        else files.push_back(entry);
    }

    for (auto& entry : dirs) {
        auto name = entry.path().filename().string();
        bool opened = ImGui::TreeNodeEx(
            (std::string(ICON_MDI_FOLDER) + " " + name).c_str(),
            ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth);

        // Accept file drops onto directories
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
                std::string src(static_cast<const char*>(payload->Data));
                auto dest = entry.path() / std::filesystem::path(src).filename();
                if (src != dest.string()) {
                    std::error_code ec;
                    std::filesystem::rename(src, dest, ec);
                }
            }
            ImGui::EndDragDropTarget();
        }

        if (opened) {
            draw_directory_tree(entry.path(), server);
            ImGui::TreePop();
        }
    }

    for (auto& entry : files) {
        auto name = entry.path().filename().string();
        auto ext = entry.path().extension().string();
        const char* icon = get_file_icon(ext);

        // Build display label (may include outdated indicator)
        std::string label = std::string(icon) + " " + name;

        bool is_helios_asset = s_helios_exts.count(ext) > 0;

        // Task 7B: Show yellow warning dot if source is outdated
        // Only check for visible Helios binary assets.
        bool source_outdated = false;
        if (is_helios_asset && server) {
            // Compute path relative to asset root for is_source_outdated
            auto rel = std::filesystem::relative(entry.path(), server->root());
            if (!rel.empty() && rel.string().find("..") != 0) {
                source_outdated = server->is_source_outdated(rel);
            }
        }

        if (source_outdated) {
            label += " " ICON_MDI_ALERT;
        }

        ImGui::TreeNodeEx(label.c_str(),
            ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen |
            ImGuiTreeNodeFlags_SpanAvailWidth);

        // Yellow color for the warning icon text
        if (source_outdated) {
            // Draw a tooltip so the user knows what the icon means
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Source file has changed since last import");
            }
        }

        // Double-click on a source file opens the import modal
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
            if (!is_helios_asset && server) {
                auto ext_no_dot = ext.empty() ? "" : ext.substr(1);
                if (server->detect_import_type(ext_no_dot)) {
                    open_import_for(entry.path(), server);
                }
            }
        }

        // Right-click context menu for files
        if (ImGui::BeginPopupContextItem()) {
            // Check if this is a source file that can be imported
            if (server && !ext.empty()) {
                auto ext_no_dot = ext.substr(1);
                if (server->detect_import_type(ext_no_dot)) {
                    if (ImGui::MenuItem(ICON_MDI_IMPORT " Import...")) {
                        open_import_for(entry.path(), server);
                    }
                }
            }

            // Check if this is a Helios binary asset
            if (is_helios_asset) {
                if (ImGui::MenuItem(ICON_MDI_EXPORT " Export...")) {
                    open_export_for(entry.path(), server);
                }
                if (ImGui::MenuItem(ICON_MDI_REFRESH " Reimport from source")) {
                    if (server) {
                        // Read the asset header to get _source_path
                        std::ifstream f(entry.path(), std::ios::binary | std::ios::ate);
                        if (f.is_open()) {
                            auto size = f.tellg();
                            f.seekg(0, std::ios::beg);
                            std::vector<uint8_t> bytes(static_cast<size_t>(size));
                            f.read(reinterpret_cast<char*>(bytes.data()), size);

                            auto header = read_asset_header(bytes.data(), bytes.size());
                            if (header) {
                                auto src_it = header->metadata.find("_source_path");
                                if (src_it != header->metadata.end()) {
                                    auto rel = std::filesystem::relative(
                                        entry.path(), server->root());
                                    server->reimport_asset(rel, src_it->second);
                                    HELIOS_LOG(Editor, Info,
                                               "Reimported '{}' from source '{}'",
                                               name, src_it->second);
                                } else {
                                    HELIOS_LOG(Editor, Warn,
                                               "No _source_path in asset metadata for '{}'",
                                               name);
                                }
                            }
                        }
                    }
                }
            }

            ImGui::EndPopup();
        }

        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
            auto path_str = entry.path().string();
            ImGui::SetDragDropPayload("ASSET_PATH", path_str.c_str(), path_str.size() + 1);
            ImGui::Text("%s %s", icon, name.c_str());
            ImGui::EndDragDropSource();
        }
    }
}

void content_browser_panel(World& world) {
    auto* state = world.try_resource<EditorState>();
    if (!state || !state->show_content_browser) return;

    auto* project = world.try_resource<Project>();

    // Get the AssetServer if available
    AssetServer* server = nullptr;
    if (world.has_resource<std::shared_ptr<AssetServer>>()) {
        server = world.resource<std::shared_ptr<AssetServer>>().get();
    }

    ImGui::Begin("Content Browser");

    if (!project || project->project_dir.empty()) {
        ImGui::Text("No project loaded");
        ImGui::End();
        return;
    }

    // "Show All Files" toggle at the top
    ImGui::Checkbox("Show All Files", &s_show_all_files);
    ImGui::Separator();

    bool root_open = ImGui::TreeNodeEx(
        (std::string(ICON_MDI_BRIEFCASE " ") + project->name).c_str(),
        ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth);

    if (root_open) {
        draw_directory_tree(project->project_dir, server);
        ImGui::TreePop();
    }

    // Draw modals (must be at same level as the window)
    import_modal(world, s_import_state);
    export_modal(world, s_export_state);

    ImGui::End();
}

} // namespace helios::editor
