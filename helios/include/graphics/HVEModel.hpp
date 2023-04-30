#pragma once
#include "HVEDevice.hpp"
#include "graphics/HVEBuffer.hpp"

// libs
#define GML_FORCE_RADIANS
#define GML_FORCE_DEPTH_ZERO_TO_ONE
#include <memory>
#include <glm/glm.hpp>
#include <vector>

namespace hve
{
	class HVEModel
	{

	public:

		struct Vertex
		{
			glm::vec3 position;
			glm::vec3 color;
			glm::vec2 texCoord;

			static std::vector<VkVertexInputBindingDescription> getBindingDescriptions();
			static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();
		};

		struct Builder
		{
			std::vector<Vertex> vertices{};
			std::vector<uint32_t> indices{};
		};

		HVEModel(HVEDevice &device, const HVEModel::Builder &builder);
		~HVEModel();

		HVEModel(const HVEModel&) = delete;
		HVEModel& operator=(const HVEModel&) = delete;

		void bind(VkCommandBuffer commandBuffer);
		void draw(VkCommandBuffer commandBuffer);

	private:

		void createVertexBuffers(const std::vector<Vertex>& vertices);
		void createIndexBuffer(const std::vector<uint32_t>& indices);

		HVEDevice& hveDevice;

		std::unique_ptr<HVEBuffer> vertexBuffer;
		uint32_t vertexCount;

		bool hasIndexBuffer = false;
		std::unique_ptr<HVEBuffer> indexBuffer;
		uint32_t indexCount;
	};
}