#pragma once

#include "HVECamera.hpp"

// lib

#include <vulkan/vulkan.h>

namespace hve
{
	struct FrameInfo
	{
		int frameIndex;
		float frameTime;
		VkCommandBuffer commandBuffer;
		HVECamera& camera;
		VkDescriptorSet globalDescriptorSet;
	};
}