#pragma once

#include "helios/graphics/HVEPipeline.h"


#include "helios/graphics/HVEDevice.h"
#include "helios/world/HVEScene.h"
#include "helios/systems/HVESystem.h"
#include "helios/textures/HVETextureManager.h"

#include <memory>
#include <vector>

#include "helios/graphics/HVEFrameInfo.h"

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


