#pragma once
#include <unordered_map>
#include "enums/HVEComponentEnum.h"

namespace hve
{
    template <typename T>
    class HVEComponentManager
    {
    public:
        void addComponentToEntity(int entityId, T component)
        {
            componentStorage[entityId] = component;
        }

        void removeComponentFromEntity(int entityId)
        {
            componentStorage.erase(entityId);
        }

        T* getComponent(int entityId)
        {
            auto it = componentStorage.find(entityId);
            if (it != componentStorage.end())
            {
                return &(it->second);
            }
            return nullptr;
        }

        std::unordered_map<int, T>& getStorage() { return componentStorage; }

    private:
        std::unordered_map<int, T> componentStorage;
    };
}