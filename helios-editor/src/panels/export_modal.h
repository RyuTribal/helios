#pragma once
#include <helios/ecs/world.h>
#include <string>
#include <vector>

namespace helios::editor {

struct ExportModalState {
    bool open = false;
    std::string asset_path;            // path to Helios binary asset
    std::vector<std::string> formats;  // available export formats
    int selected_format = 0;
};

void export_modal(World& world, ExportModalState& state);

} // namespace helios::editor
