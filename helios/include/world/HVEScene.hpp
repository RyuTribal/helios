#pragma once

#include "HVEEntityManager.hpp"
#include "HVEComponentManager.hpp"
#include "systems/HVESystem.hpp"

#include <vector>
#include <memory>
#include <tuple>
#include <type_traits>

#include "components/HVECameraComponent.hpp"
#include <components/HVEScriptComponent.hpp>
#include <components/HVEColliderComponent.hpp>

#include "components/HVEAnimComponent.hpp"
#include "components/HVEColorComponent.hpp"
#include "components/HVEModelComponent.hpp"
#include "components/HVETextureComponent.hpp"
#include "components/HVETransformComponent.hpp"
#include "enums/HVESystemStages.h"
#include "graphics/HVERenderer.hpp"
#include "textures/HVETextureManager.hpp"

namespace hve
{

    class HVEScene
    {
    public:
        static HVEScene createScene(HVEWindow& window, HVEDevice& hveDevice, HVERenderer& hveRenderer)
        {
            static id_t currentId = 0;
            return HVEScene{ currentId++, window, hveDevice, hveRenderer };
        }

        static HVEScene createScene(std::string name, HVEWindow& window, HVEDevice& hveDevice, HVERenderer& hveRenderer)
        {
            static id_t currentId = 0;
            return HVEScene{ currentId++, name, window, hveDevice, hveRenderer };
        }

        HVEScene(const HVEScene&) = delete;
        HVEScene& operator=(const HVEScene&) = delete;
        HVEScene(HVEScene&&) = default;
        HVEScene& operator=(HVEScene&&) = default;

        using id_t = unsigned int;
        HVEScene(id_t sceneId, HVEWindow& window, HVEDevice& hveDevice, HVERenderer& hveRenderer) : id{ sceneId }, sceneName{ "Scene_" + std::to_string(sceneId) }, hveWindow{ window }, hveRenderer{ hveRenderer }
        {
            textureManager = std::make_shared<HVETextureManager>(hveDevice);
        }
        HVEScene(id_t sceneId, std::string name, HVEWindow& window, HVEDevice& hveDevice, HVERenderer& hveRenderer) : id{ sceneId }, sceneName{ name }, hveWindow{ window }, hveRenderer{ hveRenderer }
        {
            textureManager = std::make_shared<HVETextureManager>(hveDevice);
        }

        void addSystem(std::shared_ptr<HVESystem> system, HVESystemStages stage);

        void removeSystem(std::shared_ptr<HVESystem> system, HVESystemStages stage);

        // Update the scene and all its systems
        void update(float dt, const std::vector<VkDescriptorSet>& globalDescriptorSets, const std::vector<std::unique_ptr<HVEBuffer>>& uboBuffers);

        template<typename T>
        HVEComponentManager<T>& getComponentManager()
        {
            return std::get<HVEComponentManager<T>>(componentManagers);
        }

        int createEntity()
        {
            return entityManager.createEntity();
        }

        void removeEntity(int entityId);

        template<typename Component>
        void addComponentToEntity(int entityId, Component component)
        {
            getComponentManager<Component>().addComponentToEntity(entityId, component);
        }

        template<typename Component>
        void removeComponentFromEntity(int entityId)
        {
            getComponentManager<Component>().removeComponentFromEntity(entityId);
        }

        // Templates are actually cool, like damn
        template<typename Component>
        Component& getEntityComponent(int enitityId)
        {
            return *getComponentManager<Component>().getComponent(enitityId);
        }

        id_t getSceneId() { return id; }
        std::string getSceneName() { return sceneName; }
        std::shared_ptr <HVETextureManager> getTextureManager() { return textureManager; }
        HVEWindow& getWindow() { return hveWindow; }
        void setPlayerControlledCamera(int entityId) { playerControllerCameraId = entityId; }
        HVECameraComponent& getPlayerControlledCamera() { return getEntityComponent<HVECameraComponent>(playerControllerCameraId); }
        int getPlayerControlledCameraId() { return playerControllerCameraId; }
        HVERenderer& getRenderer() { return hveRenderer; }
    private:
        id_t id;
        std::string sceneName;
        HVEEntityManager entityManager;
        std::tuple<
            HVEComponentManager<HVEAnimComponent>,
            HVEComponentManager<HVECameraComponent>,
            HVEComponentManager<HVEColliderComponent>,
			HVEComponentManager<HVEColorComponent>,
            HVEComponentManager<HVEModelComponent>,
            HVEComponentManager<HVEScriptComponent>,
            HVEComponentManager<HVETextureComponent>,
            HVEComponentManager<HVETransformComponent>
        > componentManagers;
        std::vector<std::shared_ptr<HVESystem>> preRenderSystems;
        std::vector<std::shared_ptr<HVESystem>> renderSystems;
        std::vector<std::shared_ptr<HVESystem>> postRenderSystems;

        template <typename... ComponentManagers, std::size_t... Is>
        void removeComponentsFromEntityImpl(int entityId, std::tuple<ComponentManagers...>& managers, std::index_sequence<Is...>) {
            (std::get<Is>(managers).removeComponentFromEntity(entityId), ...);
        }

        void removeComponentsFromEntity(int entityId) {
            constexpr auto componentManagerCount = std::tuple_size_v<decltype(componentManagers)>;
            removeComponentsFromEntityImpl(entityId, componentManagers, std::make_index_sequence<componentManagerCount>());
        }

        HVEWindow& hveWindow;
        HVERenderer& hveRenderer;
        std::shared_ptr <HVETextureManager> textureManager;
        id_t playerControllerCameraId = 0;

    };
}
