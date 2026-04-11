#pragma once

#include <helios/ecs/app.h>
#include <filesystem>

namespace helios::editor {

struct EditorPluginConfig {
    std::filesystem::path project_path;
};

struct EditorPlugin {
    EditorPluginConfig config;
    void build(App& app);
};

} // namespace helios::editor
