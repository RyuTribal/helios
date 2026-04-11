#include "helios/input/input_plugin.h"
#include "helios/input/raw_input.h"
#include "helios/input/input_map.h"
#include "helios/input/input_systems.h"
#include "helios/window/windows.h"
#include "helios/ecs/app.h"
#include "helios/core/log_macros.h"

HELIOS_DECLARE_LOG_CHANNEL(Input);

namespace helios {

void InputPlugin::build(App& app) {
    HELIOS_LOG(Input, Info, "Building InputPlugin");

    app.insert_resource<RawInput>(RawInput{});
    app.insert_resource<InputMap>(InputMap{});

    auto raw_input_id = app.add_system(Schedule::PreUpdate, update_raw_input, "update_raw_input")
        .after(app.id_of("poll_window_events"))
        .id();
    app.add_system(Schedule::PreUpdate, update_action_map, "update_action_map")
        .after(raw_input_id);

    HELIOS_LOG(Input, Info, "InputPlugin built: RawInput and InputMap resources registered");
}

} // namespace helios
