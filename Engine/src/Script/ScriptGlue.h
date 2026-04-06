#pragma once

#include "NativeEngineAPI.h"

namespace Engine {

    class ScriptGlue
    {
    public:
        static void FillNativeAPI(NativeEngineAPI& api);
    };
}
