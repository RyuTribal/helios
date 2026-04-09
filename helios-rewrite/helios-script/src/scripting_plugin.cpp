// helios-script/src/scripting_plugin.cpp
#include <helios/script/scripting_plugin.h>

#include <helios/ecs/app.h>
#include <helios/ecs/world.h>
#include <helios/ecs/ecs.h>
#include "script_log.h"
#include <helios/script/script_runtime.h>
#include <helios/script/script_instance.h>

#include "coreclr_runtime.h"
#include "script_execution_system.h"
#include "hot_reload_system.h"
#include "file_watcher.h"

#include <memory>

namespace helios {

void ScriptingPlugin::build(App& app) {
    auto& world = app.world();

    // -- Create CoreCLR runtime -----------------------------------------------
    try {
        auto runtime = std::make_unique<CoreCLRRuntime>(
            config.runtime_config_path,
            config.script_core_dll_path,
            world);

        // Load the game assembly if specified
        if (!config.app_assembly_path.empty()) {
            runtime->load_assembly(config.app_assembly_path);
            world.insert_resource(ScriptAssemblyPath{config.app_assembly_path});
        }

        // Insert as resource (stored as std::unique_ptr<ScriptRuntime>)
        world.insert_resource<std::unique_ptr<ScriptRuntime>>(std::move(runtime));

    } catch (const std::exception& e) {
        HELIOS_LOG(Script, Error,
            "Failed to initialize C# scripting: {}. "
            "Scripting will be disabled.", e.what());
        // Insert a null runtime so systems can safely check
        world.insert_resource<std::unique_ptr<ScriptRuntime>>(nullptr);
    }

    // -- File watcher for hot reload ------------------------------------------
    if (!config.watch_directory.empty()) {
        auto* runtime_ptr = world.try_resource<std::unique_ptr<ScriptRuntime>>();
        if (runtime_ptr && *runtime_ptr) {
            // Capture a raw pointer to the runtime for the callback.
            // Safe because: runtime outlives the watcher (both are in World),
            // and watcher's jthread stops before World destruction.
            ScriptRuntime* rt = runtime_ptr->get();

            auto watcher = std::make_unique<FileWatcher>(
                config.watch_directory,
                std::vector<std::string>{".cs", ".dll"},
                [rt](const std::filesystem::path& changed) {
                    HELIOS_LOG(Script, Info, "File changed: {}", changed.string());
                    rt->request_reload();
                });

            // Store watcher as a resource so it lives as long as World
            world.insert_resource(std::move(watcher));
        }
    }

    // -- Register systems -----------------------------------------------------
    // Systems use ResMut to access the ScriptRuntime resource and Query for
    // the ScriptInstance component. We capture a World* for systems that need
    // full World access (hot reload, create/destroy involve archetype moves).
    World* w = &world;

    // Hot reload check runs early each frame
    app.add_system(Schedule::PreUpdate,
        [w]() { check_script_reload(*w); },
        "check_script_reload");

    // Instance creation runs before execution
    app.add_system(Schedule::Update,
        [w]() { script_create_system(*w); },
        "script_create_system");

    // Script execution (batched by type)
    app.add_system(Schedule::Update,
        [w]() { script_execution_system(*w); },
        "script_execution_system");

    // Collision dispatch (reads ContactEvent, calls OnCollisionEnter on scripts)
    app.add_system(Schedule::Update,
        [w]() { script_collision_dispatch_system(*w); },
        "script_collision_dispatch");

    // Cleanup on despawn
    app.add_system(Schedule::PostUpdate,
        [w]() { script_destroy_system(*w); },
        "script_destroy_system");

    HELIOS_LOG(Script, Info, "ScriptingPlugin registered");
}

} // namespace helios
