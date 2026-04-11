#pragma once
#include <helios/ecs/world.h>
#include <helios/assets/asset_binary.h>
#include <string>

namespace helios::editor {

struct ImportModalState {
    bool open = false;
    std::string source_path;           // absolute path to source file
    AssetBinaryType detected_type = AssetBinaryType::Unknown;
    AssetMetadata metadata;            // editable metadata shown in modal
    std::string dest_filename;         // suggested output filename
    std::string dest_dir_override;     // custom output directory (empty = auto)
};

void import_modal(World& world, ImportModalState& state);

} // namespace helios::editor
