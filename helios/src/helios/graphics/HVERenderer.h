#pragma once

#include "helios/graphics/HVEWindow.h"



#include "helios/graphics/HVEDevice.h"
#include "helios/graphics/HVESwapChain.h"

#include <memory>
#include <vector>
#include <cassert>

namespace hve {
	class HVERenderer
	{

	public:

		HVERenderer(HVEWindow& window, HVEDevice& device);
		~HVERenderer();

		HVERenderer(const HVERenderer&) = delete;
		HVERenderer& operator=(const HVERenderer&) = delete;

		VkRenderPass getSwapChainRenderPass() const { return hveSwapChain->getRenderPass(); }
		float getAspectRatio() const { return hveSwapChain->extentAspectRatio(); }
		bool isFrameInProgress() const { return isFrameStarted; }

		VkCommandBuffer getCurrentCommandBuffer() const
		{
			assert(isFrameStarted && "Cannot get command buffer when frame not in progress");
			return commandBuffers[currentFrameIndex];
		}

		int getFrameIndex() const
		{
			assert(isFrameStarted && "Cannot get frame index when frame not in progress");
			return currentFrameIndex;
		}

		VkCommandBuffer beginFrame();
		void endFrame();
		void beingSwapChainRenderPass(VkCommandBuffer commandBuffer);
		void endSwapChainRenderPass(VkCommandBuffer commandBuffer);
	private:
		void createCommandBuffers();
		void freeCommandBuffers();
		void drawFrame();
		void recreateSwapChain();

		HVEWindow &hveWindow;
		HVEDevice &hveDevice;
		std::unique_ptr<HVESwapChain> hveSwapChain;
		std::vector<VkCommandBuffer> commandBuffers;

		uint32_t currentImageIndex;
		int currentFrameIndex{ 0 };
		bool isFrameStarted{ false };
	};
}


