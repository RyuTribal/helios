#include "textures/HVETextureManager.hpp"

#include <cassert>
#include <iostream>

namespace hve
{
	HVETextureManager::HVETextureManager(HVEDevice& hveDevice) : hveDevice(hveDevice)
	{
	}

	HVETextureManager::~HVETextureManager()
	{
	}

	HVETexture* HVETextureManager::loadTexture(const std::string& filePath)
	{
		if(textureCache.find(filePath) == textureCache.end())
		{
			textureCache.emplace(std::piecewise_construct,
				std::forward_as_tuple(filePath),
				std::forward_as_tuple(hveDevice, filePath));
		}
		return &textureCache.at(filePath);
	}



	void HVETextureManager::createTextureDescriptorSets()
	{
		assert(textureCache.size() > 0 && "Cannot create a descriptor pool without any textures loaded");
		textureDescriptorPool = HVEDescriptorPool::Builder(hveDevice)
			.setMaxSets(textureCache.size())
			.addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, textureCache.size())
			.build();

		textureDescriptorLayout = HVEDescriptorSetLayout::Builder(hveDevice)
			.addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
			.build();

		textureDescriptorSets.resize(textureCache.size());
		int i = 0;
		for (auto it = textureCache.begin(); it != textureCache.end(); ++it)
		{
			HVEDescriptorWriter descriptorWriter(*textureDescriptorLayout, *textureDescriptorPool);
			VkDescriptorImageInfo imageInfo{};
			imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			imageInfo.imageView = it->second.getImageView();
			imageInfo.sampler = it->second.getImageSampler();
			descriptorWriter.writeImage(1, &imageInfo);

			if (!descriptorWriter.build(textureDescriptorSets[i])) {
				throw std::runtime_error("Failed to build texture descriptor set");
			}
			++i;
		}
	}

}