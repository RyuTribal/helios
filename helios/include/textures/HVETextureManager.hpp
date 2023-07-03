#pragma once
#include <mutex>
#include <unordered_map>

#include "HVETexture.hpp"
#include "graphics/HVEDescriptors.hpp"
#include "graphics/HVEDevice.hpp"


namespace hve
{
	class HVETextureManager
	{
	public:
		HVETextureManager(HVEDevice& hveDevice);
		~HVETextureManager();

		HVETexture* loadTexture(const std::string& filePath, VkSamplerAddressMode addressMode);
		HVETexture* getTexture(const std::string& filePath) { return &textureCache.at(filePath); }

		VkDescriptorSet& getDescriptorSetById(int index) { return textureDescriptorSets[index];}
		std::vector<VkDescriptorSet>& getDescriptionSets() { return textureDescriptorSets; }
		HVEDescriptorSetLayout* getDescriptionLayout() { return textureDescriptorLayout.get();  }

		void createTextureDescriptorSets();
		auto size() const { return textureCache.size(); }

	private:
		HVEDevice& hveDevice;
		std::mutex texture_manager_lock;
		std::unordered_map<std::string, HVETexture> textureCache;
		std::unique_ptr<HVEDescriptorPool> textureDescriptorPool{};
		std::unique_ptr<HVEDescriptorSetLayout> textureDescriptorLayout{};
		std::vector<VkDescriptorSet> textureDescriptorSets;
	};
}
