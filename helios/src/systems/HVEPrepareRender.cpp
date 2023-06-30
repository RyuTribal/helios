#include "systems/HVEPrepareRender.hpp"

#include <iostream>

#include "world/HVEScene.hpp"

namespace hve
{
	void HVEPrepareRender::onUpdate(FrameInfo& frameInfo, HVEScene& scene)
	{
		scene.getModelManager()->updateFrameIndex(frameInfo.frameIndex);
		// Get the component managers from the scene
		auto& transformManager = scene.getComponentManager<HVETransformComponent>();
		auto& spriteManager = scene.getComponentManager<HVESpriteComponent>();
		auto& colorManager = scene.getComponentManager<HVEColorComponent>();
		auto& modelManager = scene.getComponentManager<HVEModelComponent>();


		// Iterate over the entities with the required components
		// TODO: this for loop takes too long to do each frame, haram
		for (auto& model_pair : *scene.getModelManager()->getMap()) {
			if (!scene.getModelManager()->getModelByShape(model_pair.first)->getInstancesAdded()) {
				for (auto& [entityId, modelComponent] : modelManager.getStorage()) {
					HVESpriteComponent* spriteComponent = spriteManager.getComponent(entityId);
					HVEColorComponent* colorComponent = colorManager.getComponent(entityId);
					HVETransformComponent* transformComponent = transformManager.getComponent(entityId);

					if (spriteComponent != nullptr) {

						HVEModel::VertexInstanceData instance{};
						instance.texCoord = *spriteComponent->sprite->getSpriteSheetCoordinates();
						if (transformComponent) {
							glm::mat4 instanceTransform = transformComponent->mat4();
							instance.transformX = instanceTransform[0];
							instance.transformY = instanceTransform[1];
							instance.transformZ = instanceTransform[2];
							instance.transformTranslation = instanceTransform[3];
						}
						else
						{
							HVETransformComponent objectTransform;
							objectTransform.translation = { 0.f, 0.f, 0.f };
							objectTransform.scale = { 1.f, 1.f, 1.f };
							glm::mat4 instanceTransform = objectTransform.mat4();
							instance.transformX = instanceTransform[0];
							instance.transformY = instanceTransform[1];
							instance.transformZ = instanceTransform[2];
							instance.transformTranslation = instanceTransform[3];
						}
						scene.getModelManager()->getModelByShape(modelComponent.model)->addInstance(instance);

					}
				}
			}
		}
	}
}
