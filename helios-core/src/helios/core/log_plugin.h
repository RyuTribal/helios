#pragma once

#include "helios/core/log_system.h"
#include "helios/ecs/app.h"

namespace helios {

struct LogPlugin {
    LogConfig config = {};

    void build(App& app) {
        app.insert_resource(LogSystem{config});
    }
};

} // namespace helios
