#include "pch.h"
#include "HostFXR.h"

#include <filesystem>

#ifdef PLATFORM_WINDOWS
    #include <Windows.h>
#else
    #include <dlfcn.h>
#endif

namespace Engine {

    void* HostFXR::s_HostFXRHandle = nullptr;
    load_assembly_and_get_function_pointer_fn HostFXR::s_LoadAssemblyFn = nullptr;

    // Internal storage for hostfxr function pointers
    static hostfxr_initialize_for_runtime_config_fn s_InitFn = nullptr;
    static hostfxr_get_runtime_delegate_fn s_GetDelegateFn = nullptr;
    static hostfxr_close_fn s_CloseFn = nullptr;
    static hostfxr_handle s_HostContext = nullptr;

    // ------------------------------------------------------------------
    // Platform helpers for dynamic library loading
    // ------------------------------------------------------------------

#ifdef PLATFORM_WINDOWS

    static void* LoadDynamicLibrary(const std::filesystem::path& path)
    {
        HMODULE h = LoadLibraryW(path.wstring().c_str());
        return static_cast<void*>(h);
    }

    static void* GetExport(void* lib, const char* name)
    {
        return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(lib), name));
    }

    static void FreeDynamicLibrary(void* lib)
    {
        if (lib)
            FreeLibrary(static_cast<HMODULE>(lib));
    }

#else // Linux

    static void* LoadDynamicLibrary(const std::filesystem::path& path)
    {
        void* h = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (!h)
            HVE_CORE_ERROR_TAG("HostFXR", "dlopen failed: {}", dlerror());
        return h;
    }

    static void* GetExport(void* lib, const char* name)
    {
        void* sym = dlsym(lib, name);
        if (!sym)
            HVE_CORE_ERROR_TAG("HostFXR", "dlsym failed for '{}': {}", name, dlerror());
        return sym;
    }

    static void FreeDynamicLibrary(void* lib)
    {
        if (lib)
            dlclose(lib);
    }

#endif

    // ------------------------------------------------------------------
    // LoadHostFXR — locate and load the hostfxr shared library
    // ------------------------------------------------------------------

    bool HostFXR::LoadHostFXR()
    {
        // ROOT_PATH points to the Engine project directory (e.g. .../helios/Engine).
        // The hostfxr library lives under Engine/vendor/dotnet/host/fxr/<version>/
        std::filesystem::path root(ROOT_PATH);
        std::filesystem::path fxr_base = root / "vendor" / "dotnet" / "host" / "fxr";

        if (!std::filesystem::exists(fxr_base))
        {
            HVE_CORE_ERROR_TAG("HostFXR", "hostfxr base directory not found: {}", fxr_base.string());
            return false;
        }

        // Find the first version subdirectory
        std::filesystem::path fxr_dir;
        for (auto& entry : std::filesystem::directory_iterator(fxr_base))
        {
            if (entry.is_directory())
            {
                fxr_dir = entry.path();
                break;
            }
        }

        if (fxr_dir.empty())
        {
            HVE_CORE_ERROR_TAG("HostFXR", "No version subdirectory found under {}", fxr_base.string());
            return false;
        }

#ifdef PLATFORM_WINDOWS
        std::filesystem::path fxr_path = fxr_dir / "hostfxr.dll";
#else
        std::filesystem::path fxr_path = fxr_dir / "libhostfxr.so";
#endif

        HVE_CORE_TRACE_TAG("HostFXR", "Loading hostfxr from: {}", fxr_path.string());

        s_HostFXRHandle = LoadDynamicLibrary(fxr_path);
        if (!s_HostFXRHandle)
        {
            HVE_CORE_ERROR_TAG("HostFXR", "Failed to load hostfxr library");
            return false;
        }

        // Resolve the three function pointers we need
        s_InitFn = reinterpret_cast<hostfxr_initialize_for_runtime_config_fn>(
            GetExport(s_HostFXRHandle, "hostfxr_initialize_for_runtime_config"));
        s_GetDelegateFn = reinterpret_cast<hostfxr_get_runtime_delegate_fn>(
            GetExport(s_HostFXRHandle, "hostfxr_get_runtime_delegate"));
        s_CloseFn = reinterpret_cast<hostfxr_close_fn>(
            GetExport(s_HostFXRHandle, "hostfxr_close"));

        if (!s_InitFn || !s_GetDelegateFn || !s_CloseFn)
        {
            HVE_CORE_ERROR_TAG("HostFXR", "Failed to resolve one or more hostfxr exports");
            Shutdown();
            return false;
        }

        HVE_CORE_TRACE_TAG("HostFXR", "hostfxr loaded successfully");
        return true;
    }

    // ------------------------------------------------------------------
    // Init — load hostfxr, initialize the runtime, get load delegate
    // ------------------------------------------------------------------

    bool HostFXR::Init(const std::filesystem::path& runtime_config_path)
    {
        if (!LoadHostFXR())
            return false;

        // Initialize the runtime from the runtimeconfig.json
        std::string config_str = runtime_config_path.string();
        int32_t rc = s_InitFn(config_str.c_str(), nullptr, &s_HostContext);
        if (rc != 0 || !s_HostContext)
        {
            HVE_CORE_ERROR_TAG("HostFXR", "hostfxr_initialize_for_runtime_config failed with 0x{:X}", static_cast<uint32_t>(rc));
            return false;
        }

        HVE_CORE_TRACE_TAG("HostFXR", "Runtime initialized from: {}", config_str);

        // Get the load_assembly_and_get_function_pointer delegate
        void* delegate = nullptr;
        rc = s_GetDelegateFn(s_HostContext, hdt_load_assembly_and_get_function_pointer, &delegate);
        if (rc != 0 || !delegate)
        {
            HVE_CORE_ERROR_TAG("HostFXR", "hostfxr_get_runtime_delegate failed with 0x{:X}", static_cast<uint32_t>(rc));
            return false;
        }

        s_LoadAssemblyFn = reinterpret_cast<load_assembly_and_get_function_pointer_fn>(delegate);
        HVE_CORE_TRACE_TAG("HostFXR", "CoreCLR runtime ready");
        return true;
    }

    // ------------------------------------------------------------------
    // GetManagedFunctionPointer
    // ------------------------------------------------------------------

    void* HostFXR::GetManagedFunctionPointer(
        const std::filesystem::path& assembly_path,
        const char* type_name,
        const char* method_name)
    {
        if (!s_LoadAssemblyFn)
        {
            HVE_CORE_ERROR_TAG("HostFXR", "Runtime not initialized — call HostFXR::Init() first");
            return nullptr;
        }

        void* fn = nullptr;
        std::string asm_str = assembly_path.string();

        int rc = s_LoadAssemblyFn(
            asm_str.c_str(),
            type_name,
            method_name,
            UNMANAGEDCALLERSONLY_METHOD,
            nullptr,
            &fn);

        if (rc != 0 || !fn)
        {
            HVE_CORE_ERROR_TAG("HostFXR",
                "Failed to get managed function pointer: {}::{} (rc=0x{:X})",
                type_name, method_name, static_cast<uint32_t>(rc));
            return nullptr;
        }

        HVE_CORE_TRACE_TAG("HostFXR", "Resolved managed method: {}::{}", type_name, method_name);
        return fn;
    }

    // ------------------------------------------------------------------
    // Shutdown — close context and free library
    // ------------------------------------------------------------------

    void HostFXR::Shutdown()
    {
        if (s_CloseFn && s_HostContext)
        {
            s_CloseFn(s_HostContext);
            s_HostContext = nullptr;
        }

        s_InitFn = nullptr;
        s_GetDelegateFn = nullptr;
        s_CloseFn = nullptr;
        s_LoadAssemblyFn = nullptr;

        FreeDynamicLibrary(s_HostFXRHandle);
        s_HostFXRHandle = nullptr;

        HVE_CORE_TRACE_TAG("HostFXR", "HostFXR shut down");
    }
}
