#pragma once

#include "graphics/HVEPipeline.hpp"
#include "graphics/HVEDevice.hpp"
#include "graphics/HVEGameObject.hpp"
#include "graphics/HVECamera.hpp"
#include "HVEFrameInfo.hpp"
#include "textures/HVETextureManager.hpp"

#include <memory>
#include <vector>

namespace hve {
	class RenderSystem
	{

	public:

		RenderSystem(HVEDevice &device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout, VkDescriptorSetLayout textureSetLayout);
		~RenderSystem();

		RenderSystem(const RenderSystem&) = delete;
		RenderSystem& operator=(const RenderSystem&) = delete;
		void renderGameObjects(FrameInfo &frameInfo, std::vector<HVEGameObject> &gameObjects, HVETextureManager& textureManager);
	private:
		void createPipelineLayout(VkDescriptorSetLayout globalSetLayout, VkDescriptorSetLayout textureSetLayoutt);
		void createPipeline(VkRenderPass renderPass);

		HVEDevice &hveDevice;
		std::unique_ptr<HVEPipeline> hvePipeline;
		VkPipelineLayout pipelineLayout;
	};
}


