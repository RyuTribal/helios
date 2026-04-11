#pragma once

#include <helios/script/native_engine_api.h>

namespace helios {

class World;

/// Fills a NativeEngineAPI struct with function pointers that
/// operate on the given World. The World pointer is stored in
/// api.world_context and passed back by C# on every call.
///
/// IMPORTANT: The World must outlive the NativeEngineAPI.
struct ScriptGlue {
    static void fill(NativeEngineAPI& api, World& world);
};

} // namespace helios
