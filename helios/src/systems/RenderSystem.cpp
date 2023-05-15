#include "systems/RenderSystem.hpp"

#define GML_FORCE_RADIANS
#define GML_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>


#include <stdexcept>
#include <array>
#include <iostream>
#include <graphics/HVEFrameInfo.hpp>

namespace hve
{

	struct SimplePushConstantData
	{
		glm::mat4 modelMatrix{ 1.f };
		alignas(16) glm::vec3 color;
	};

	RenderSystem::RenderSystem(HVEDevice& device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout, VkDescriptorSetLayout textureSetLayout) : hveDevice{ device }
	{
		createPipelineLayout(globalSetLayout, textureSetLayout);
		createPipeline(renderPass);
	}

	RenderSystem::~RenderSystem()
	{
		vkDestroyPipelineLayout(hveDevice.device(), pipelineLayout, nullptr);

	}


	void RenderSystem::createPipelineLayout(VkDescriptorSetLayout globalSetLayout, VkDescriptorSetLayout textureSetLayout)
	{

		VkPushConstantRange pushConstantRange{};
		pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
		pushConstantRange.offset = 0;
		pushConstantRange.size = sizeof(SimplePushConstantData);
		std::vector<VkDescriptorSetLayout> descriptorSetLayouts{ globalSetLayout, textureSetLayout };

		VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
		pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();
		pipelineLayoutInfo.pushConstantRangeCount = 1;
		pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

		if (vkCreatePipelineLayout(hveDevice.device(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create pipeline layout!");
		}
	}

	void RenderSystem::createPipeline(VkRenderPass renderPass)
	{
		assert(pipelineLayout != nullptr && "Cannot create pipeline before pipeline layout");

		PipelineConfigInfo pipelineConfig{};
		HVEPipeline::defaultPipelineConfigInfo(pipelineConfig);
		pipelineConfig.renderPass = renderPass;
		pipelineConfig.pipelineLayout = pipelineLayout;
		hvePipeline = std::make_unique<HVEPipeline>(
			hveDevice,
			"shaders/simple_shader.vert.spv",
			"shaders/simple_shader.frag.spv",
			pipelineConfig
		);
	}


	void RenderSystem::onUpdate(FrameInfo& frameInfo, HVEScene& scene)
	{
		hvePipeline->bind(frameInfo.commandBuffer);

		vkCmdBindDescriptorSets(
			frameInfo.commandBuffer,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			pipelineLayout,
			0,
			1,
			&frameInfo.globalDescriptorSet,
			0,
			nullptr);

		// Get the component managers from the scene
		auto& transformManager = scene.getComponentManager<HVETransformComponent>();
		auto& textureManager = scene.getComponentManager<HVETextureComponent>();
		auto& colorManager = scene.getComponentManager<HVEColorComponent>();
		auto& modelManager = scene.getComponentManager<HVEModelComponent>();

		// Iterate over the entities with the required components
		for (auto& [entityId, modelComponent] : modelManager.getStorage()) {

			HVETextureComponent* textureComponent = textureManager.getComponent(entityId);
			HVEColorComponent* colorComponent = colorManager.getComponent(entityId);
			HVETransformComponent* transformComponent = transformManager.getComponent(entityId);

			// Bind the texture descriptor set
			if (textureComponent != nullptr) {
				vkCmdBindDescriptorSets(
					frameInfo.commandBuffer,
					VK_PIPELINE_BIND_POINT_GRAPHICS,
					pipelineLayout,
					1,
					1,
					&scene.getTextureManager()->getDescriptorSetById(0),
					0,
					nullptr);
			}
			SimplePushConstantData push{};
			if (colorComponent != nullptr) {
				push.color = colorComponent->color;
			}
			else
			{
				push.color = glm::vec3{ 0.f, 0.f, 0.f };
			}
			if (transformComponent != nullptr) {
				push.modelMatrix = transformComponent->mat4();
			}
			else
			{
				// Just a default one if one hasn't been provided
				HVETransformComponent objectTransform;
				objectTransform.translation = { 0.f, 0.f, 0.f };
				objectTransform.scale = { 1.f, 1.f, 1.f };
				push.modelMatrix = objectTransform.mat4();
			}

			vkCmdPushConstants(
				frameInfo.commandBuffer,
				pipelineLayout,
				VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
				0,
				sizeof(SimplePushConstantData),
				&push);

			// Bind and draw the model
			modelComponent.model->bind(frameInfo.commandBuffer);
			modelComponent.model->draw(frameInfo.commandBuffer);
		}
	}
}
