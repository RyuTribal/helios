// helios-script/src/hot_reload_system.cpp
#include "hot_reload_system.h"

#include <helios/ecs/world.h>
#include <helios/script/script_runtime.h>
#include <helios/script/script_instance.h>
#include "script_log.h"

#include <vector>

namespace helios {

void check_script_reload(World& world) {
    auto* runtime_ptr = world.try_resource<std::unique_ptr<ScriptRuntime>>();
    if (!runtime_ptr || !*runtime_ptr) return;

    auto& runtime = **runtime_ptr;
    if (!runtime.reload_requested()) return;

    HELIOS_LOG(Script, Warn, "Hot reload triggered -- reloading scripts...");

    // -- Step 1: Snapshot all script instances ---------------------------------
    struct ScriptSnapshot {
        Entity entity;
        std::string class_name;
    };

    std::vector<ScriptSnapshot> snapshots;

    auto q = world.query<ScriptInstance, With<ScriptInitialized>>();
    for (auto [entity, script] : q.with_entity()) {
        snapshots.push_back({entity, script.script_class_name});
    }

    // -- Step 2: Destroy all managed instances --------------------------------
    runtime.destroy_all_instances();

    // Remove ScriptInitialized markers and zero out managed handles
    for (auto& snap : snapshots) {
        if (world.has<ScriptInitialized>(snap.entity)) {
            world.remove<ScriptInitialized>(snap.entity);
        }
        auto& script = world.get<ScriptInstance>(snap.entity);
        script.managed_handle = 0;
        script.script_type_id = 0;
    }

    // -- Step 3: Reload the assembly ------------------------------------------
    auto* assembly_path = world.try_resource<ScriptAssemblyPath>();
    if (!assembly_path) {
        HELIOS_LOG(Script, Error, "No ScriptAssemblyPath resource -- cannot reload");
        runtime.clear_reload_request();
        return;
    }

    if (!runtime.reload_assembly(assembly_path->path)) {
        HELIOS_LOG(Script, Error, "Assembly reload failed");
        runtime.clear_reload_request();
        return;
    }

    // -- Step 4: Re-create instances ------------------------------------------
    // script_create_system will pick them up next frame because
    // we removed ScriptInitialized and zeroed managed_handle.
    // The system will call invoke_create for each one.

    runtime.clear_reload_request();
    HELIOS_LOG(Script, Warn, "Hot reload complete -- {} scripts will be re-created",
                       snapshots.size());
}

} // namespace helios
