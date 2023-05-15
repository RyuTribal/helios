#pragma once

#include "graphics/HVEPipeline.hpp"
#include "graphics/HVEDevice.hpp"
#include "world/HVEScene.hpp"
#include "systems/HVESystem.hpp"
#include "textures/HVETextureManager.hpp"

#include <memory>
#include <vector>

#include "graphics/HVEFrameInfo.hpp"

namespace hve {
	class RenderSystem : public HVESystem
	{

	public:

		RenderSystem(HVEDevice &device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout, VkDescriptorSetLayout textureSetLayout);
		~RenderSystem() override;

		RenderSystem(const RenderSystem&) = delete;
		RenderSystem& operator=(const RenderSystem&) = delete;
		void onUpdate(FrameInfo &frameInfo, HVEScene& scene) override;
	private:
		void createPipelineLayout(VkDescriptorSetLayout globalSetLayout, VkDescriptorSetLayout textureSetLayoutt);
		void createPipeline(VkRenderPass renderPass);

		HVEDevice &hveDevice;
		std::unique_ptr<HVEPipeline> hvePipeline;
		VkPipelineLayout pipelineLayout;
	};
}


