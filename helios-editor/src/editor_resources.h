#pragma once

#include <helios/ecs/world.h>
#include <string>
#include <unordered_map>

namespace helios::editor {

struct EditorResources {
    bool loaded = false;
    // TODO: icon textures as ImTextureID mapped by name
    // For now, icons are not loaded — panels use text fallbacks
};

void load_editor_resources(World& world);

} // namespace helios::editor
