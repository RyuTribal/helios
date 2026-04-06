#include "pch.h"
#include "ScriptEngine.h"
#include "ScriptGlue.h"
#include "HostFXR.h"
#include "Scene/Entity.h"
#include "Scene/Components.h"
#include "Project/Project.h"
#include "FileWatch.hpp"

namespace Engine {

    ManagedBridge ScriptEngine::s_Bridge{};
    NativeEngineAPI ScriptEngine::s_NativeAPI{};
    Scene* ScriptEngine::s_SceneContext = nullptr;
    bool ScriptEngine::s_ShouldReload = false;
    std::vector<uint64_t> ScriptEngine::s_ActiveEntityIDs{};
    std::filesystem::path ScriptEngine::s_AppAssemblyPath{};
#ifdef PLATFORM_WINDOWS
    using WatcherString = std::wstring;
#else
    using WatcherString = std::string;
#endif
    static std::unique_ptr<filewatch::FileWatch<WatcherString>> s_WatcherHandle = nullptr;

    void ScriptEngine::Init()
    {
        // 1. Fill the native API struct with C++ function pointers
        ScriptGlue::FillNativeAPI(s_NativeAPI);

        // 2. Find paths
        std::filesystem::path root = ROOT_PATH;
        root = root.parent_path();
        std::filesystem::path runtimeConfigPath = root / "Editor/Resources/Scripts/ScriptCore.runtimeconfig.json";
        std::filesystem::path scriptCorePath = root / "Editor/Resources/Scripts/ScriptCore.dll";

        // 3. Boot CoreCLR
        if (!HostFXR::Init(runtimeConfigPath))
        {
            HVE_CORE_ERROR_TAG("ScriptEngine", "Failed to initialize CoreCLR runtime");
            return;
        }

        // 4. Get the bridge Initialize function from managed code
        using InitializeFn = void(*)(NativeEngineAPI*, ManagedBridge*);
        auto initFn = (InitializeFn)HostFXR::GetManagedFunctionPointer(
            scriptCorePath,
            "Helios.Bridge.ScriptHostBridge, ScriptCore",
            "Initialize");

        if (!initFn)
        {
            HVE_CORE_ERROR_TAG("ScriptEngine", "Failed to get ScriptHostBridge.Initialize function pointer");
            return;
        }

        // 5. Call Initialize — passes our native API, receives bridge function pointers
        initFn(&s_NativeAPI, &s_Bridge);

        HVE_CORE_TRACE_TAG("ScriptEngine", "CoreCLR scripting engine initialized");
    }

    void ScriptEngine::Shutdown()
    {
        if (s_Bridge.DestroyAllInstances)
            s_Bridge.DestroyAllInstances();
        if (s_Bridge.UnloadAppAssembly)
            s_Bridge.UnloadAppAssembly();

        s_WatcherHandle.reset();
        HostFXR::Shutdown();

        s_Bridge = {};
        s_NativeAPI = {};
        s_SceneContext = nullptr;
        s_ShouldReload = false;
        s_ActiveEntityIDs.clear();
    }

    void ScriptEngine::LoadAppAssembly(const std::filesystem::path& filepath)
    {
        if (!s_Bridge.LoadAppAssembly)
        {
            HVE_CORE_ERROR_TAG("ScriptEngine", "Bridge not initialized");
            return;
        }

        s_AppAssemblyPath = filepath;
        std::string pathStr = filepath.string();
        s_Bridge.LoadAppAssembly(pathStr.c_str());

        // Set up file watcher for hot-reload
        s_WatcherHandle = std::make_unique<filewatch::FileWatch<WatcherString>>(
            filepath,
            [](const auto& file, filewatch::Event eventType) {
                s_ShouldReload = true;
            });

        HVE_CORE_TRACE_TAG("ScriptEngine", "Loaded app assembly: {}", filepath.string());
    }

    void ScriptEngine::UnloadAppAssembly()
    {
        s_ActiveEntityIDs.clear();
        if (s_Bridge.DestroyAllInstances)
            s_Bridge.DestroyAllInstances();
        if (s_Bridge.UnloadAppAssembly)
            s_Bridge.UnloadAppAssembly();
        s_WatcherHandle.reset();
    }

    void ScriptEngine::ReloadAssembly(const std::filesystem::path& app_assembly_path)
    {
        UnloadAppAssembly();
        LoadAppAssembly(app_assembly_path);
        s_ShouldReload = false;
        HVE_CORE_WARN_TAG("ScriptEngine", "Reloaded Scripts");
    }

    void ScriptEngine::OnRuntimeStart(Scene* scene)
    {
        s_SceneContext = scene;
    }

    void ScriptEngine::OnRuntimeStop()
    {
        s_ActiveEntityIDs.clear();
        if (s_Bridge.DestroyAllInstances)
            s_Bridge.DestroyAllInstances();
        s_SceneContext = nullptr;
    }

    bool ScriptEngine::EntityClassExists(const std::string& full_class_name)
    {
        if (!s_Bridge.EntityClassExists) return false;
        return s_Bridge.EntityClassExists(full_class_name.c_str());
    }

    void ScriptEngine::OnCreateEntityClass(Entity* entity)
    {
        auto script_component = entity->GetComponent<ScriptComponent>();
        if (!script_component) return;

        const std::string& name = script_component->Name;
        if (name.empty()) return;

        if (!s_Bridge.CreateInstance) return;

        if (s_Bridge.CreateInstance(name.c_str(), entity->GetID()))
        {
            if (s_Bridge.InvokeOnCreate)
                s_Bridge.InvokeOnCreate(entity->GetID());
            s_ActiveEntityIDs.push_back(entity->GetID());
        }
    }

    void ScriptEngine::OnUpdate(float delta_time)
    {
        HVE_PROFILE_FUNC();
        if (!s_Bridge.InvokeOnUpdate) return;

        for (uint64_t id : s_ActiveEntityIDs)
        {
            s_Bridge.InvokeOnUpdate(id, delta_time);
        }
    }

    bool ScriptEngine::ShouldReload()
    {
        return s_ShouldReload;
    }

    void ScriptEngine::MarkForReload()
    {
        s_ShouldReload = true;
    }

    Scene* ScriptEngine::GetSceneContext()
    {
        return s_SceneContext;
    }

    std::vector<std::string> ScriptEngine::GetEntityClassNames()
    {
        std::vector<std::string> names;
        if (!s_Bridge.GetEntityClassCount || !s_Bridge.GetEntityClassName)
            return names;

        int count = s_Bridge.GetEntityClassCount();
        names.reserve(count);

        char buffer[256];
        for (int i = 0; i < count; i++)
        {
            s_Bridge.GetEntityClassName(i, buffer, sizeof(buffer));
            names.emplace_back(buffer);
        }

        return names;
    }

    void ScriptEngine::InvokeCollisionCallback(uint64_t entityId, const char* methodName, uint64_t otherEntityId)
    {
        // TODO: Add InvokeCollisionCallback to ManagedBridge in a future task.
        // For now this is a no-op stub so that HPhysicsScene.cpp compiles.
        (void)entityId;
        (void)methodName;
        (void)otherEntityId;
    }
}
