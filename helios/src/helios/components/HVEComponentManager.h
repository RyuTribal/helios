#pragma once
#include <unordered_map>
#include "helios/enums/HVEComponentEnum.h"
#include <mutex>

namespace hve
{
    template <typename T>
    class HVEComponentManager
    {
    public:
        void addComponentToEntity(int entityId, T component)
        {
            std::lock_guard<std::mutex> lock(component_manager_lock);
            componentStorage[entityId] = component;
        }

        void removeComponentFromEntity(int entityId)
        {
            std::lock_guard<std::mutex> lock(component_manager_lock);
            componentStorage.erase(entityId);
        }

        T* getComponent(int entityId)
        {
            std::lock_guard<std::mutex> lock(component_manager_lock);
            auto it = componentStorage.find(entityId);
            if (it != componentStorage.end())
            {
                return &(it->second);
            }
            return nullptr;
        }

        std::unordered_map<int, T>& getStorage() { return componentStorage; }

    private:
        std::mutex component_manager_lock;
        std::unordered_map<int, T> componentStorage;
    };
}