#pragma once

#include "graphics/HVEPipeline.hpp"
#include "graphics/HVEDevice.hpp"
#include "graphics/HVEGameObject.hpp"
#include "graphics/HVECamera.hpp"

#include <memory>
#include <vector>

namespace hve {
	class RenderSystem
	{

	public:

		RenderSystem(HVEDevice &device, VkRenderPass renderPass);
		~RenderSystem();

		RenderSystem(const RenderSystem&) = delete;
		RenderSystem& operator=(const RenderSystem&) = delete;
		void renderGameObjects(VkCommandBuffer commandBuffer, std::vector<HVEGameObject> &gameObjects, const HVECamera &camera);
	private:
		void createPipelineLayout();
		void createPipeline(VkRenderPass renderPass);

		HVEDevice &hveDevice;
		std::unique_ptr<HVEPipeline> hvePipeline;
		VkPipelineLayout pipelineLayout;
	};
}


