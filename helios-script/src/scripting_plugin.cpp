#include <helios/script/scripting_plugin.h>
#include <helios/script/script_sound_registry.h>

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
    auto runtime = CoreCLRRuntime::create(
        config.runtime_config_path,
        config.script_core_dll_path,
        world);

    if (runtime && !config.app_assembly_path.empty()) {
        runtime->load_assembly(config.app_assembly_path);
        world.insert_resource(ScriptAssemblyPath{config.app_assembly_path});
    }

    if (!runtime) {
        HELIOS_LOG(Script, Error, "C# scripting disabled (runtime init failed)");
    }

    world.insert_resource<std::unique_ptr<ScriptRuntime>>(std::move(runtime));
    world.insert_resource(ScriptExecutionState{});
    world.insert_resource(ScriptSoundRegistry{});
    world.insert_resource(ScriptAliveTracker{});

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
    // Registered as direct World& functions so the scheduler sees their
    // access descriptors (Exclusive) and serializes them correctly.

    // Shutdown: stop file watcher before runtime is destroyed
    app.add_system(Schedule::Shutdown,
        [](World& w) {
            if (auto* watcher = w.try_resource<std::unique_ptr<FileWatcher>>()) {
                watcher->reset();
            }
        },
        "script_watcher_shutdown");

    // Hot reload, create, execute, collision dispatch, destroy
    app.add_system(Schedule::PreUpdate, check_script_reload, "check_script_reload");
    app.add_system(Schedule::Update, script_create_system, "script_create_system");
    app.add_system(Schedule::Update, script_execution_system, "script_execution_system");
    app.add_system(Schedule::Update, script_collision_dispatch_system, "script_collision_dispatch");
    app.add_system(Schedule::PostUpdate, script_destroy_system, "script_destroy_system");

    HELIOS_LOG(Script, Info, "ScriptingPlugin registered");
}

} // namespace helios
