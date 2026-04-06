#pragma once

#include <filesystem>
#include <hostfxr.h>
#include <coreclr_delegates.h>

namespace Engine {

    class HostFXR
    {
    public:
        // Initialize the CoreCLR runtime using the given runtimeconfig.json
        static bool Init(const std::filesystem::path& runtime_config_path);
        static void Shutdown();

        // Get a function pointer to a managed [UnmanagedCallersOnly] static method.
        // assembly_path: full path to the .dll
        // type_name: "Namespace.Class, AssemblyName" format
        // method_name: method name
        // Returns native function pointer, or nullptr on failure.
        static void* GetManagedFunctionPointer(
            const std::filesystem::path& assembly_path,
            const char* type_name,
            const char* method_name);

    private:
        static bool LoadHostFXR();
        static void* s_HostFXRHandle;
        static load_assembly_and_get_function_pointer_fn s_LoadAssemblyFn;
    };
}
