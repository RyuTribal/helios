#pragma once

#include <imgui.h>
#include <filesystem>
#include <string>

namespace helios::editor {

/// Convert an absolute asset path to a path relative to root.
inline std::string make_relative(const std::string& abs_path, const std::filesystem::path& root) {
    return std::filesystem::relative(abs_path, root).string();
}

/// Two-column label helper: begin a label+widget row.
/// @param label   Text drawn in the left column.
/// @param label_width  Width of the label column in pixels.
inline void begin_field(const char* label, float label_width = 100.0f) {
    ImGui::Columns(2);
    ImGui::SetColumnWidth(0, label_width);
    ImGui::Text("%s", label);
    ImGui::NextColumn();
}

/// End a two-column field row.
inline void end_field() {
    ImGui::Columns(1);
}

} // namespace helios::editor
