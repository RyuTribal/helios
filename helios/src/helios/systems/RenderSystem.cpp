#include "hvepch.h"
#include "helios/systems/RenderSystem.h"




#define GML_FORCE_RADIANS
#define GML_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

#include "helios/graphics/HVEFrameInfo.h"

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

		vkCmdBindDescriptorSets(
			frameInfo.commandBuffer,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			pipelineLayout,
			1,
			1,
			&scene.getTextureManager()->getDescriptorSetById(0),
			0,
			nullptr);
		
		scene.getModelManager()->buildAllInstances();
		// Bind and draw the model
		scene.getModelManager()->getModelByShape(Shapes::Quad)->bind(frameInfo.commandBuffer);
		scene.getModelManager()->getModelByShape(Shapes::Quad)->draw(frameInfo.commandBuffer);
	}
}
