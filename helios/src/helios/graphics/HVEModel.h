#pragma once
#include "HVEDevice.h"


#include "helios/graphics/HVEBuffer.h"

// libs
#define GML_FORCE_RADIANS
#define GML_FORCE_DEPTH_ZERO_TO_ONE
#include <memory>
#include <glm/glm.hpp>
#include <vector>

#include "HVESwapChain.h"

namespace hve
{
	class HVEModel
	{

	public:

		struct Vertex
		{
			glm::vec3 position;

			static std::vector<VkVertexInputBindingDescription> getBindingDescriptions();
			static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();
		};

		struct VertexInstanceData
		{
			glm::vec4 transformX;
			glm::vec4 transformY;
			glm::vec4 transformZ;
			glm::vec4 transformTranslation;

			glm::vec3 color;
			glm::vec2 texCoord;
		};

		struct Builder
		{
			std::vector<Vertex> vertices{};
			std::vector<uint32_t> indices{};
			std::vector<VertexInstanceData> instances{};
		};

		HVEModel(HVEDevice &device, HVEModel::Builder &builder);
		~HVEModel();

		HVEModel(const HVEModel&) = delete;
		HVEModel& operator=(const HVEModel&) = delete;

		void addInstance(VertexInstanceData& instanceData);
		void clearInstances();
		void bind(VkCommandBuffer commandBuffer);
		void draw(VkCommandBuffer commandBuffer);
		void buildCurrentInstances();
		void setCurrentFrame(int frameIndex) { currentFrame = frameIndex; }
		size_t instanceSize() { return builder.instances.size(); }
		bool getInstanceBuilt() { return instancesBuilt[currentFrame]; }
		bool getInstancesAdded() { return instancesAdded; }

	private:
		void createVertexBuffers(const std::vector<Vertex>& vertices);
		void createIndexBuffer(const std::vector<uint32_t>& indices);
		void createInstanceBuffer(const std::vector<VertexInstanceData>& instances);

		HVEDevice& hveDevice;
		std::unique_ptr<HVEBuffer> vertexBuffer;
		uint32_t vertexCount;

		bool hasIndexBuffer = false;
		std::unique_ptr<HVEBuffer> indexBuffer;
		uint32_t indexCount;

		Builder builder;
		std::unique_ptr<HVEBuffer> instanceBuffers[HVESwapChain::MAX_FRAMES_IN_FLIGHT];
		uint32_t instanceCount;
		std::vector<VertexInstanceData> newInstances{};

		int currentFrame = 0;
		bool INSTANCE_BASE[HVESwapChain::MAX_FRAMES_IN_FLIGHT] = { false, false };
		bool instancesBuilt[HVESwapChain::MAX_FRAMES_IN_FLIGHT] = { false, false };
		bool instancesAdded = false;
	};
}
