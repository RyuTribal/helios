#pragma once

#include "ManagedBridge.h"
#include "NativeEngineAPI.h"

namespace Engine {

    class Scene;
    class Entity;

    class ScriptEngine
    {
    public:
        static void Init();
        static void Shutdown();

        static void LoadAppAssembly(const std::filesystem::path& filepath);
        static void UnloadAppAssembly();
        static void ReloadAssembly(const std::filesystem::path& app_assembly_path);

        static void OnRuntimeStart(Scene* scene);
        static void OnRuntimeStop();

        static bool EntityClassExists(const std::string& full_class_name);
        static void OnCreateEntityClass(Entity* entity);
        static void OnUpdate(float delta_time);

        static bool ShouldReload();
        static void MarkForReload();

        static Scene* GetSceneContext();
        static std::vector<std::string> GetEntityClassNames();

        static void InvokeCollisionCallback(uint64_t entityId, const char* methodName, uint64_t otherEntityId);

    private:
        static ManagedBridge s_Bridge;
        static NativeEngineAPI s_NativeAPI;
        static Scene* s_SceneContext;
        static bool s_ShouldReload;
        static std::vector<uint64_t> s_ActiveEntityIDs;
        static std::filesystem::path s_AppAssemblyPath;
    };
}
