#include "hvepch.h"
#include "helios/world/HVEScene.h"



namespace hve
{
	struct GlobalUbo
	{
		glm::mat4 model;
		glm::mat4 view;
		glm::mat4 proj;
	};
	void HVEScene::addSystem(std::shared_ptr<HVESystem> system, HVESystemStages stage)
	{
		switch(stage)
		{
		case HVESystemStages::PreRender:
			preRenderSystems.push_back(system);
			break;

		case HVESystemStages::DuringRender:
			renderSystems.push_back(system);
			break;

		case HVESystemStages::PostRender:
			postRenderSystems.push_back(system);
			break;

		default:
			preRenderSystems.push_back(system);
		}
	}
	void HVEScene::removeSystem(std::shared_ptr<HVESystem> system, HVESystemStages stage)
	{
		auto removeSystemLambda = [&system](const std::shared_ptr<HVESystem> sys) {
			return sys.get() == system.get();
		};

		switch (stage)
		{
		case HVESystemStages::PreRender:
			preRenderSystems.erase(
				std::remove_if(preRenderSystems.begin(), preRenderSystems.end(), removeSystemLambda),
				preRenderSystems.end()
			);
			break;

		case HVESystemStages::DuringRender:
			renderSystems.erase(
				std::remove_if(renderSystems.begin(), renderSystems.end(), removeSystemLambda),
				renderSystems.end()
			);
			break;

		case HVESystemStages::PostRender:
			postRenderSystems.erase(
				std::remove_if(postRenderSystems.begin(), postRenderSystems.end(), removeSystemLambda),
				postRenderSystems.end()
			);
			break;

		default:
			preRenderSystems.erase(
				std::remove_if(preRenderSystems.begin(), preRenderSystems.end(), removeSystemLambda),
				preRenderSystems.end()
			);
		}
	}
	void HVEScene::update(float dt, const std::vector<VkDescriptorSet>& globalDescriptorSets, const std::vector<std::unique_ptr<HVEBuffer>>& uboBuffers)
	{
		if (auto commandBuffer = hveRenderer.beginFrame())
		{
			// If the beginFrame was successful, proceed with rendering.

			int frameIndex = hveRenderer.getFrameIndex();
			FrameInfo frameInfo{
				frameIndex,
				dt,
				commandBuffer,
				getPlayerControlledCamera().camera,
				globalDescriptorSets[frameIndex]
			};
			// Pre-render update
			for (const auto& system : preRenderSystems)
			{
				system->onUpdate(frameInfo, *this);
			}
			// update
			GlobalUbo ubo{};
			ubo.view = getPlayerControlledCamera().camera.getProjection() * getPlayerControlledCamera().camera.getView();
			uboBuffers[frameIndex]->writeToBuffer(&ubo);
			uboBuffers[frameIndex]->flush();

			// Begin rendering
			hveRenderer.beingSwapChainRenderPass(commandBuffer);

			// During-render update
			for (const auto& system : renderSystems)
			{
				system->onUpdate(frameInfo, *this);
			}

			// End rendering
			hveRenderer.endSwapChainRenderPass(commandBuffer);
			hveRenderer.endFrame();

			// Post-render update
			for (const auto& system : postRenderSystems)
			{
				system->onUpdate(frameInfo, *this);
			}
		}
		else
		{
			// Handle error if beginFrame was unsuccessful.
		}
	}
	void HVEScene::removeEntity(int enitityId)
	{
		removeComponentsFromEntity(enitityId);
		entityManager.deleteEntity(enitityId);
	}
}