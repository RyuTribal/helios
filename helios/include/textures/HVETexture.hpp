#pragma once
#include <cstdint>
#include <memory>
#include <vulkan/vulkan_core.h>

#include "graphics/HVEBuffer.hpp"
#include "graphics/HVEDevice.hpp"
#include "stb_image_wrapper.h"


namespace hve
{
	class HVETexture
	{

	public:
		HVETexture(HVEDevice& device, const std::string& filePath, VkSamplerAddressMode addressMode);
		HVETexture(HVEDevice& device, const std::string& filePath, std::shared_ptr<HVETexture> previous, VkSamplerAddressMode addressMode);
		~HVETexture();

		HVETexture(const HVETexture&) = delete;
		HVETexture& operator=(const HVETexture&) = delete;

		VkFormat channelsToVKFormat(int channels);

		void createImage();
		void createImageView();
		void createImageSampler(VkSamplerAddressMode addressMode);
		void createBuffer();
		void transitionImageLayout(VkImageLayout oldLayout, VkImageLayout newLayout);
		void copyBufferToImage();


		VkImage getImage() const { return texture; }
		VkImageView getImageView() const { return textureView; }
		VkSampler getImageSampler() const { return textureSampler; }
		VkDeviceSize getImageSize() const { return width * height * 4; }
		int getWidth() const { return width; }
		int getHeight() const { return height; }

	private:
		int width;
		int height;
		int channels;
		VkFormat format;
		stbi_uc* data;

		VkImage texture;
		VkDeviceMemory textureDeviceMemory;

		VkImageView textureView;

		VkSampler textureSampler;
		HVEDevice& hveDevice;
		std::unique_ptr<HVEBuffer> textureBuffer;


	};
}
