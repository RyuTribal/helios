// helios-core/src/helios/input/input_plugin.h
#pragma once

namespace helios {

class App; // forward declaration

struct InputPlugin {
    void build(App& app);
};

} // namespace helios
