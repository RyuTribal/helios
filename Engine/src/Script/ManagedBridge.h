#pragma once
#include <cstdint>

namespace Engine {

    // Function pointers received FROM C# (the managed bridge).
    // C++ calls these to interact with the script runtime.
    struct ManagedBridge
    {
        // Assembly management
        void (*LoadAppAssembly)(const char* path);
        void (*UnloadAppAssembly)();

        // Class discovery
        int  (*GetEntityClassCount)();
        void (*GetEntityClassName)(int index, char* buffer, int bufferSize);
        bool (*EntityClassExists)(const char* fullName);

        // Instance lifecycle
        bool (*CreateInstance)(const char* className, uint64_t entityId);
        void (*DestroyInstance)(uint64_t entityId);
        void (*InvokeOnCreate)(uint64_t entityId);
        void (*InvokeOnUpdate)(uint64_t entityId, float deltaTime);
        void (*DestroyAllInstances)();

        // Component checks
        bool (*EntityHasComponent)(uint64_t entityId, const char* componentName);
    };
}
