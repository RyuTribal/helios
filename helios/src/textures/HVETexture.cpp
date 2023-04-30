#include "textures/HVETexture.hpp"

#include <cassert>
#include <stdexcept>
#include <iostream>


namespace hve {
	HVETexture::HVETexture(HVEDevice& device, const std::string& filePath) : hveDevice{ device }
	{

		data = stbi_load(filePath.c_str(), &width, &height, &channels, STBI_rgb_alpha);

		assert(data && "Texture error::Couldn't load texture");

		//format = channelsToVKFormat(channels);
		// Not all formats are supported by all GPU's
		// VK_FORMAT_R8G8B8A8_SRGB is the safe option
		format = VK_FORMAT_R8G8B8A8_SRGB;

		createImage();
		createImageView();
		createImageSampler();
		createBuffer();

		// Transition image layout and copy buffer to image
		transitionImageLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
		copyBufferToImage();
		transitionImageLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		stbi_image_free(data);
	}

	HVETexture::~HVETexture()
	{
		vkDestroySampler(hveDevice.device(), textureSampler, nullptr);
		vkDestroyImageView(hveDevice.device(), textureView, nullptr);
		vkDestroyImage(hveDevice.device(), texture, nullptr);
		vkFreeMemory(hveDevice.device(), textureDeviceMemory, nullptr);
	}

	VkFormat HVETexture::channelsToVKFormat(int channels)
	{
        switch (channels) {
	        case 1:
	            return VK_FORMAT_R8_SRGB;
	        case 2:
	            return VK_FORMAT_R8G8_SRGB;
	        case 3:
	            return VK_FORMAT_R8G8B8_SRGB;
	        case 4:
	            return VK_FORMAT_R8G8B8A8_SRGB;
	        default:
	            // Handle error for unsupported number of channels
	            return VK_FORMAT_UNDEFINED;
        }
	}

	void HVETexture::createImage()
	{
		VkImageCreateInfo imageInfo{};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.extent.width = static_cast<uint32_t>(width);
		imageInfo.extent.height = static_cast<uint32_t>(height);
		imageInfo.extent.depth = 1;
		imageInfo.mipLevels = 1;
		imageInfo.arrayLayers = 1;
		imageInfo.format = format;
		imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

		// Choose appropriate memory properties for your texture, e.g., DEVICE_LOCAL
		VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

		hveDevice.createImageWithInfo(imageInfo, properties, texture, textureDeviceMemory);
	}

	void HVETexture::createImageView()
	{
		VkImageViewCreateInfo viewInfo{};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = texture;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = format;
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;

		if (vkCreateImageView(hveDevice.device(), &viewInfo, nullptr, &textureView) != VK_SUCCESS) {
			throw std::runtime_error("Texture error::failed to create texture image view!");
		}
		
	}
	void HVETexture::createImageSampler()
	{
		VkSamplerCreateInfo samplerInfo{};
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerInfo.magFilter = VK_FILTER_LINEAR;
		samplerInfo.minFilter = VK_FILTER_LINEAR;
		samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerInfo.anisotropyEnable = VK_TRUE;
		samplerInfo.maxAnisotropy = hveDevice.properties.limits.maxSamplerAnisotropy;
		samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
		samplerInfo.unnormalizedCoordinates = VK_FALSE;
		samplerInfo.compareEnable = VK_FALSE;
		samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerInfo.mipLodBias = 0.0f;
		samplerInfo.minLod = 0.0f;
		samplerInfo.maxLod = 0.0f;

		if(vkCreateSampler(hveDevice.device(), &samplerInfo, nullptr, &textureSampler) != VK_SUCCESS)
		{
			throw std::runtime_error("Texture error::failed to create texture sampler!");
		}

	}

	void HVETexture::createBuffer()
	{
		textureBuffer = std::make_unique <HVEBuffer>(
			hveDevice,
			getImageSize(),
			1,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
		);

		// Map buffer memory and copy image data
		VkResult result = textureBuffer->map();
		if (result != VK_SUCCESS) {
			throw std::runtime_error("Failed to map texture buffer memory!");
		}

		void* mappedData = textureBuffer->getMappedMemory();
		memcpy(mappedData, data, static_cast<size_t>(getImageSize()));
		textureBuffer->unmap();
	}

	void HVETexture::transitionImageLayout(VkImageLayout oldLayout, VkImageLayout newLayout) {
		VkCommandBuffer commandBuffer = hveDevice.beginSingleTimeCommands();

		VkImageMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = oldLayout;
		barrier.newLayout = newLayout;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = texture;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.levelCount = 1;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;

		VkPipelineStageFlags sourceStage;
		VkPipelineStageFlags destinationStage;

		if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
			barrier.srcAccessMask = 0;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

			sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		}
		else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		}
		else {
			throw std::invalid_argument("Unsupported layout transition.");
		}

		vkCmdPipelineBarrier(
			commandBuffer,
			sourceStage, destinationStage,
			0,
			0, nullptr,
			0, nullptr,
			1, &barrier
		);

		hveDevice.endSingleTimeCommands(commandBuffer);
	}

	void HVETexture::copyBufferToImage()
	{
		VkCommandBuffer commandBuffer = hveDevice.beginSingleTimeCommands();

		VkBufferImageCopy region{};
		region.bufferOffset = 0;
		region.bufferRowLength = 0; // Specifying 0 means tightly packed data
		region.bufferImageHeight = 0;

		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel = 0;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = 1;

		region.imageOffset = { 0, 0, 0 };
		region.imageExtent = {
			static_cast<uint32_t>(width),
			static_cast<uint32_t>(height),
			1
		};

		vkCmdCopyBufferToImage(
			commandBuffer,
			textureBuffer->getBuffer(),
			texture,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1,
			&region
		);

		hveDevice.endSingleTimeCommands(commandBuffer);
	}



}
