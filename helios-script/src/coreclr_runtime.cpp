#include "coreclr_runtime.h"
#include <helios/script/script_glue.h>
#include "script_log.h"

#include <string>

#ifdef _WIN32
    #include <Windows.h>
#else
    #include <dlfcn.h>
#endif

// Forward-declare hostfxr types we need
using hostfxr_initialize_for_runtime_config_fn =
    int32_t(*)(const char* runtime_config_path, void* parameters, hostfxr_handle* host_context_handle);
using hostfxr_get_runtime_delegate_fn =
    int32_t(*)(hostfxr_handle host_context_handle, int type, void** delegate);
using hostfxr_close_fn = int32_t(*)(hostfxr_handle host_context_handle);

// hdt_load_assembly_and_get_function_pointer = 5
static constexpr int HDT_LOAD_ASSEMBLY = 5;
// UNMANAGEDCALLERSONLY_METHOD sentinel — the hosting API expects (const char*)-1
// to indicate [UnmanagedCallersOnly] methods, not a type name string.
static const char* UNMANAGEDCALLERSONLY = reinterpret_cast<const char*>(static_cast<intptr_t>(-1));

namespace helios {

// -- Platform helpers ---------------------------------------------------------

namespace {

void* load_dynamic_library(const std::filesystem::path& path) {
#ifdef _WIN32
    return static_cast<void*>(LoadLibraryW(path.wstring().c_str()));
#else
    void* h = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!h)
        HELIOS_LOG(Script, Error, "dlopen failed: {}", dlerror());
    return h;
#endif
}

void* get_export(void* lib, const char* name) {
#ifdef _WIN32
    return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(lib), name));
#else
    void* sym = dlsym(lib, name);
    if (!sym)
        HELIOS_LOG(Script, Error, "dlsym failed for '{}': {}", name, dlerror());
    return sym;
#endif
}

void free_dynamic_library(void* lib) {
    if (!lib) return;
#ifdef _WIN32
    FreeLibrary(static_cast<HMODULE>(lib));
#else
    dlclose(lib);
#endif
}

} // anonymous namespace

// -- Factory ------------------------------------------------------------------

std::unique_ptr<CoreCLRRuntime> CoreCLRRuntime::create(
    const std::filesystem::path& runtime_config_path,
    const std::filesystem::path& script_core_path,
    World& world)
{
    std::unique_ptr<CoreCLRRuntime> rt(new CoreCLRRuntime());

    ScriptGlue::fill(rt->m_native_api, world);

    if (!rt->load_hostfxr()) {
        HELIOS_LOG(Script, Error, "CoreCLRRuntime: failed to load hostfxr");
        return nullptr;
    }

    auto init_fn = reinterpret_cast<hostfxr_initialize_for_runtime_config_fn>(
        get_export(rt->m_hostfxr_handle, "hostfxr_initialize_for_runtime_config"));
    auto get_delegate_fn = reinterpret_cast<hostfxr_get_runtime_delegate_fn>(
        get_export(rt->m_hostfxr_handle, "hostfxr_get_runtime_delegate"));
    rt->m_close_fn = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(
        get_export(rt->m_hostfxr_handle, "hostfxr_close")));

    if (!init_fn || !get_delegate_fn || !rt->m_close_fn) {
        HELIOS_LOG(Script, Error, "CoreCLRRuntime: failed to resolve hostfxr exports");
        return nullptr;
    }

    std::string config_str = runtime_config_path.string();
    int32_t rc = init_fn(config_str.c_str(), nullptr, &rt->m_host_context);
    if (rc != 0 || !rt->m_host_context) {
        HELIOS_LOG(Script, Error,
            "CoreCLRRuntime: hostfxr_initialize_for_runtime_config failed (rc=0x{:x})",
            static_cast<uint32_t>(rc));
        return nullptr;
    }

    void* delegate = nullptr;
    rc = get_delegate_fn(rt->m_host_context, HDT_LOAD_ASSEMBLY, &delegate);
    if (rc != 0 || !delegate) {
        HELIOS_LOG(Script, Error, "CoreCLRRuntime: failed to get load_assembly delegate");
        return nullptr;
    }
    rt->m_load_assembly_fn = reinterpret_cast<load_assembly_and_get_function_pointer_fn>(delegate);

    using InitializeFn = void(*)(NativeEngineAPI*, ManagedBridge*);
    auto init_bridge = reinterpret_cast<InitializeFn>(
        rt->get_managed_fn(script_core_path,
                           "Helios.Bridge.ScriptHostBridge, ScriptCore",
                           "Initialize"));
    if (!init_bridge) {
        HELIOS_LOG(Script, Error, "CoreCLRRuntime: failed to resolve ScriptHostBridge.Initialize");
        return nullptr;
    }

    init_bridge(&rt->m_native_api, &rt->m_bridge);

    HELIOS_LOG(Script, Info, "C# scripting runtime initialized");
    return rt;
}

// -- Destructor ---------------------------------------------------------------

CoreCLRRuntime::~CoreCLRRuntime() {
    // Destroy all managed instances
    if (m_bridge.DestroyAllInstances)
        m_bridge.DestroyAllInstances();

    // Unload app assembly
    if (m_bridge.UnloadAppAssembly)
        m_bridge.UnloadAppAssembly();

    // Close hostfxr context
    if (m_close_fn && m_host_context) {
        reinterpret_cast<hostfxr_close_fn>(m_close_fn)(m_host_context);
        m_host_context = nullptr;
    }

    // Free dynamic library
    free_dynamic_library(m_hostfxr_handle);
    m_hostfxr_handle = nullptr;

    HELIOS_LOG(Script, Info, "C# scripting runtime shut down");
}

// -- load_hostfxr -------------------------------------------------------------

bool CoreCLRRuntime::load_hostfxr() {
    // Locate hostfxr under the system .NET SDK
    std::filesystem::path fxr_base =
        std::filesystem::path(HELIOS_DOTNET_ROOT) / "host" / "fxr";

    if (!std::filesystem::exists(fxr_base)) {
        HELIOS_LOG(Script, Error, "hostfxr base not found: {}", fxr_base.string());
        return false;
    }

    // Find first version subdirectory
    std::filesystem::path fxr_dir;
    for (auto& entry : std::filesystem::directory_iterator(fxr_base)) {
        if (entry.is_directory()) { fxr_dir = entry.path(); break; }
    }
    if (fxr_dir.empty()) {
        HELIOS_LOG(Script, Error, "No version subdir under {}", fxr_base.string());
        return false;
    }

#ifdef _WIN32
    auto fxr_path = fxr_dir / "hostfxr.dll";
#else
    auto fxr_path = fxr_dir / "libhostfxr.so";
#endif

    m_hostfxr_handle = load_dynamic_library(fxr_path);
    if (!m_hostfxr_handle) {
        HELIOS_LOG(Script, Error, "Failed to load hostfxr from {}", fxr_path.string());
        return false;
    }

    HELIOS_LOG(Script, Info, "Loaded hostfxr from {}", fxr_path.string());
    return true;
}

// -- get_managed_fn -----------------------------------------------------------

void* CoreCLRRuntime::get_managed_fn(
    const std::filesystem::path& assembly_path,
    const char* type_name,
    const char* method_name)
{
    if (!m_load_assembly_fn) return nullptr;

    void* fn = nullptr;
    std::string asm_str = assembly_path.string();

    int rc = m_load_assembly_fn(
        asm_str.c_str(), type_name, method_name,
        UNMANAGEDCALLERSONLY,
        nullptr, &fn);

    if (rc != 0 || !fn) {
        HELIOS_LOG(Script, Error,
            "Failed to resolve {}::{} (rc=0x{:X})",
            type_name, method_name, static_cast<uint32_t>(rc));
        return nullptr;
    }
    return fn;
}

// -- Assembly management ------------------------------------------------------

bool CoreCLRRuntime::load_assembly(const std::filesystem::path& assembly_path) {
    if (!m_bridge.LoadAppAssembly) return false;
    m_app_assembly_path = assembly_path;
    std::string path_str = assembly_path.string();
    m_bridge.LoadAppAssembly(path_str.c_str());
    HELIOS_LOG(Script, Info, "Loaded app assembly: {}", path_str);
    return true;
}

void CoreCLRRuntime::unload_assembly() {
    if (m_bridge.DestroyAllInstances) m_bridge.DestroyAllInstances();
    if (m_bridge.UnloadAppAssembly)   m_bridge.UnloadAppAssembly();
}

bool CoreCLRRuntime::reload_assembly(const std::filesystem::path& assembly_path) {
    unload_assembly();
    return load_assembly(assembly_path);
}

// -- Class discovery ----------------------------------------------------------

bool CoreCLRRuntime::class_exists(const std::string& full_class_name) const {
    if (!m_bridge.EntityClassExists) return false;
    return m_bridge.EntityClassExists(full_class_name.c_str());
}

std::vector<std::string> CoreCLRRuntime::get_script_class_names() const {
    std::vector<std::string> names;
    if (!m_bridge.GetEntityClassCount || !m_bridge.GetEntityClassName)
        return names;

    int count = m_bridge.GetEntityClassCount();
    names.reserve(static_cast<size_t>(count));

    char buffer[256];
    for (int i = 0; i < count; i++) {
        m_bridge.GetEntityClassName(i, buffer, sizeof(buffer));
        names.emplace_back(buffer);
    }
    return names;
}

uint32_t CoreCLRRuntime::get_script_type_id(const std::string& full_class_name) const {
    if (!m_bridge.GetScriptTypeId) {
        // Fallback: hash the class name
        return static_cast<uint32_t>(std::hash<std::string>{}(full_class_name));
    }
    return m_bridge.GetScriptTypeId(full_class_name.c_str());
}

// -- Instance lifecycle -------------------------------------------------------

uint64_t CoreCLRRuntime::invoke_create(const std::string& class_name, uint64_t entity_id) {
    if (!m_bridge.CreateInstance) return 0;

    uint64_t handle = m_bridge.CreateInstance(class_name.c_str(), entity_id);
    if (handle != 0 && m_bridge.InvokeOnCreate) {
        m_bridge.InvokeOnCreate(entity_id);
    }
    return handle;
}

void CoreCLRRuntime::invoke_update(
    uint32_t script_type_id,
    const uint64_t* entity_ids,
    const uint64_t* managed_handles,
    size_t count,
    float delta)
{
    if (!m_bridge.InvokeOnUpdateBatch) return;
    m_bridge.InvokeOnUpdateBatch(
        script_type_id, entity_ids, managed_handles,
        static_cast<int>(count), delta);
}

void CoreCLRRuntime::invoke_destroy(uint64_t entity_id, uint64_t managed_handle) {
    if (m_bridge.InvokeOnDestroy) {
        m_bridge.InvokeOnDestroy(entity_id, managed_handle);
    }
}

void CoreCLRRuntime::destroy_all_instances() {
    if (m_bridge.DestroyAllInstances) m_bridge.DestroyAllInstances();
}

void CoreCLRRuntime::invoke_on_collision(
    uint64_t entity_id,
    uint64_t other_entity_id,
    float px, float py, float pz,
    float nx, float ny, float nz,
    float impulse)
{
    if (m_bridge.InvokeOnCollision) {
        m_bridge.InvokeOnCollision(entity_id, other_entity_id,
                                   px, py, pz, nx, ny, nz, impulse);
    }
}

// -- Hot reload ---------------------------------------------------------------

void CoreCLRRuntime::request_reload() {
    m_reload_requested.store(true, std::memory_order_release);
}

bool CoreCLRRuntime::reload_requested() const {
    return m_reload_requested.load(std::memory_order_acquire);
}

void CoreCLRRuntime::clear_reload_request() {
    m_reload_requested.store(false, std::memory_order_release);
}

} // namespace helios
