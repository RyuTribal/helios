#include "editor_resources.h"

#include <helios/ecs/world.h>

namespace helios::editor {

void load_editor_resources(World& world) {
    auto* res = world.try_resource<EditorResources>();
    if (res) res->loaded = true;
    // TODO: load icon textures for play/stop buttons, content browser, etc.
}

} // namespace helios::editor
