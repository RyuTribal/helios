#include "graphics/HVEModel.hpp"

#include <cassert>
#include <cstring>
#include <stdexcept>

namespace hve
{

	HVEModel::HVEModel(HVEDevice& device, HVEModel::Builder &builder) : hveDevice{ device }, builder{builder}
	{
		createVertexBuffers(builder.vertices);
		createIndexBuffer(builder.indices);
		// createInstanceBuffer(builder.instances);
	}


	HVEModel::~HVEModel()
	{
	}

	void HVEModel::createVertexBuffers(const std::vector<Vertex>& vertices)
	{
		vertexCount = static_cast<uint32_t>(vertices.size());
		assert(vertexCount >= 3 && "Vertex count must be at least 3");
		VkDeviceSize bufferSize = sizeof(vertices[0]) * vertexCount;
		uint32_t vertexSize = sizeof(vertices[0]);

		HVEBuffer stagingBuffer{
			hveDevice,
			vertexSize,
			vertexCount,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,

		};

		stagingBuffer.map();
		stagingBuffer.writeToBuffer((void*)vertices.data());

		vertexBuffer = std::make_unique<HVEBuffer>(
			hveDevice,
			vertexSize,
			vertexCount,
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		hveDevice.copyBuffer(stagingBuffer.getBuffer(), vertexBuffer->getBuffer(), bufferSize);
	}

	void HVEModel::createIndexBuffer(const std::vector<uint32_t>& indices)
	{
		indexCount = static_cast<uint32_t>(indices.size());
		hasIndexBuffer = indexCount > 0;

		if(!hasIndexBuffer)
		{
			return;
		}

		VkDeviceSize bufferSize = sizeof(indices[0]) * indexCount;
		uint32_t indexSize = sizeof(indices[0]);

		HVEBuffer stagingBuffer{
			hveDevice,
			indexSize,
			indexCount,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		};

		stagingBuffer.map();
		stagingBuffer.writeToBuffer((void*)indices.data());

		indexBuffer = std::make_unique<HVEBuffer>(
			hveDevice,
			indexSize,
			indexCount,
			VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
		);

		hveDevice.copyBuffer(stagingBuffer.getBuffer(), indexBuffer->getBuffer(), bufferSize);
	}

	void HVEModel::createInstanceBuffer(const std::vector<VertexInstanceData>& instances)
	{
		instancesAdded = true;
		instanceCount = static_cast<uint32_t>(instances.size());

		if (instanceCount <= 0)
		{
			return;
		}
		uint32_t instanceSize = sizeof(instances[0]);
		VkDeviceSize bufferSize = instanceSize * instanceCount;

		HVEBuffer stagingBuffer{
			hveDevice,
			instanceSize,
			instanceCount,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		};

		stagingBuffer.map();
		stagingBuffer.writeToBuffer((void*)instances.data());

		instanceBuffers[currentFrame] = std::make_unique<HVEBuffer>(
			hveDevice,
			instanceSize,
			instanceCount,
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
		);

		hveDevice.copyBuffer(stagingBuffer.getBuffer(), instanceBuffers[currentFrame]->getBuffer(), bufferSize);
		instancesBuilt[currentFrame] = true;
		
	}



	void HVEModel::clearInstances()
	{
		builder.instances = {};
		std::copy(std::begin(INSTANCE_BASE), std::end(INSTANCE_BASE), std::begin(instancesBuilt));
		instancesAdded = false;
	}

	void HVEModel::draw(VkCommandBuffer commandBuffer)
	{
		if(hasIndexBuffer)
		{
			vkCmdDrawIndexed(commandBuffer, indexCount, instanceCount, 0, 0, 0);
		}
		else
		{
			vkCmdDraw(commandBuffer, vertexCount, instanceCount, 0, 0);
		}
	}

	void HVEModel::buildCurrentInstances()
	{
		createInstanceBuffer(builder.instances);
	}

	void HVEModel::addInstance(VertexInstanceData& instanceData)
	{
		builder.instances.push_back(instanceData);
		instancesAdded = false;
	}

	void HVEModel::bind(VkCommandBuffer commandBuffer)
	{
		VkBuffer buffers[] = { vertexBuffer->getBuffer(), instanceBuffers[currentFrame] ? instanceBuffers[currentFrame]->getBuffer() : nullptr};

		VkDeviceSize offsets[] = { 0, 0 };
		vkCmdBindVertexBuffers(commandBuffer, 0, 2, buffers, offsets);

		if(hasIndexBuffer)
		{
			vkCmdBindIndexBuffer(commandBuffer, indexBuffer->getBuffer(), 0, VK_INDEX_TYPE_UINT32);
		}

	}

	std::vector<VkVertexInputBindingDescription> HVEModel::Vertex::getBindingDescriptions()
	{
		std::vector<VkVertexInputBindingDescription> bindingDescriptions(2);

		// Binding for Vertex data
		bindingDescriptions[0].binding = 0;
		bindingDescriptions[0].stride = sizeof(Vertex);
		bindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		// Binding for instance data
		bindingDescriptions[1].binding = 1;
		bindingDescriptions[1].stride = sizeof(VertexInstanceData);
		bindingDescriptions[1].inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;

		return bindingDescriptions;
	}

	std::vector<VkVertexInputAttributeDescription> HVEModel::Vertex::getAttributeDescriptions()
	{
		std::vector<VkVertexInputAttributeDescription> attributeDescriptions(7);

		attributeDescriptions[0].binding = 0;
		attributeDescriptions[0].location = 0;
		attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[0].offset = offsetof(Vertex, position);

		attributeDescriptions[1].binding = 1;
		attributeDescriptions[1].location = 1;
		attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[1].offset = offsetof(VertexInstanceData, color);

		attributeDescriptions[2].binding = 1;
		attributeDescriptions[2].location = 2;
		attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
		attributeDescriptions[2].offset = offsetof(VertexInstanceData, texCoord);

		attributeDescriptions[3].binding = 1;
		attributeDescriptions[3].location = 3;
		attributeDescriptions[3].format = VK_FORMAT_R32G32B32A32_SFLOAT;
		attributeDescriptions[3].offset = offsetof(VertexInstanceData, transformX);

		attributeDescriptions[4].binding = 1;
		attributeDescriptions[4].location = 4;
		attributeDescriptions[4].format = VK_FORMAT_R32G32B32A32_SFLOAT;
		attributeDescriptions[4].offset = offsetof(VertexInstanceData, transformY);

		attributeDescriptions[5].binding = 1;
		attributeDescriptions[5].location = 5;
		attributeDescriptions[5].format = VK_FORMAT_R32G32B32A32_SFLOAT;
		attributeDescriptions[5].offset = offsetof(VertexInstanceData, transformZ);

		attributeDescriptions[6].binding = 1;
		attributeDescriptions[6].location = 6;
		attributeDescriptions[6].format = VK_FORMAT_R32G32B32A32_SFLOAT;
		attributeDescriptions[6].offset = offsetof(VertexInstanceData, transformTranslation);

		return attributeDescriptions;
		
	}


}
