#include "graphics/HVERenderer.hpp"


#include <stdexcept>
#include <array>

namespace hve
{

	HVERenderer::HVERenderer(HVEWindow &window, HVEDevice &device)
	: hveWindow{window}, hveDevice{device}
	{
		recreateSwapChain();
		createCommandBuffers();
	}

	HVERenderer::~HVERenderer()
	{
		freeCommandBuffers();
	}

	void HVERenderer::recreateSwapChain()
	{
		auto extent = hveWindow.getExtent();
		while (extent.width == 0 || extent.height == 0)
		{
			extent = hveWindow.getExtent();
			glfwWaitEvents();
		}

		vkDeviceWaitIdle(hveDevice.device());
		if (hveSwapChain == nullptr) {
			hveSwapChain = std::make_unique<HVESwapChain>(hveDevice, extent);

		}
		else
		{
			std::shared_ptr<HVESwapChain> oldSwapChain = std::move(hveSwapChain);
			hveSwapChain = std::make_unique<HVESwapChain>(hveDevice, extent, oldSwapChain);

			if(!oldSwapChain->compareSwapFormats(*hveSwapChain.get()))
			{
				throw std::runtime_error("Swap chain image(or depth) format has changed!");
			}
		}
		// come back later
	}


	void HVERenderer::createCommandBuffers()
	{
		commandBuffers.resize(HVESwapChain::MAX_FRAMES_IN_FLIGHT);

		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandPool = hveDevice.getCommandPool();

		allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

		if (vkAllocateCommandBuffers(hveDevice.device(), &allocInfo, commandBuffers.data()) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to allocate command buffers");
		}
	}

	void HVERenderer::freeCommandBuffers()
	{
		vkFreeCommandBuffers(hveDevice.device(), hveDevice.getCommandPool(), static_cast<uint32_t>(commandBuffers.size()), commandBuffers.data());
		commandBuffers.clear();
	}

	VkCommandBuffer HVERenderer::beginFrame()
	{
		assert(!isFrameStarted && "Can't call beginFrame while already in progress");
		auto result = hveSwapChain->acquireNextImage(&currentImageIndex);

		if (result == VK_ERROR_OUT_OF_DATE_KHR)
		{
			recreateSwapChain();
			return nullptr;
		}

		if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
		{
			throw std::runtime_error("Failed to acquire swap chain image!");
		}

		isFrameStarted = true;
		auto commandBuffer = getCurrentCommandBuffer();

		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

		if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to begin recording command buffer");
		}

		return commandBuffer;
	}

	void HVERenderer::endFrame()
	{
		assert(isFrameStarted && "Can't call endFrame while frame is not in progress");
		auto commandBuffer = getCurrentCommandBuffer();
		if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to record command buffer");
		}
		auto result = hveSwapChain->submitCommandBuffers(&commandBuffer, &currentImageIndex);

		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || hveWindow.wasWindowResized())
		{
			hveWindow.resetWindowResizedFlag();
			recreateSwapChain();
		}

		else if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to present swap chain image!");
		}

		isFrameStarted = false;
		currentFrameIndex = (currentFrameIndex + 1) % HVESwapChain::MAX_FRAMES_IN_FLIGHT;
	}

	void HVERenderer::beingSwapChainRenderPass(VkCommandBuffer commandBuffer)
	{
		assert(isFrameStarted && "Can't call beginSwapChainRenderPass if frame is not in progress");
		assert(commandBuffer == getCurrentCommandBuffer() && "Can't begin render pass on command buffer from a different frame");
		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = hveSwapChain->getRenderPass();

		renderPassInfo.framebuffer = hveSwapChain->getFrameBuffer(currentImageIndex);

		renderPassInfo.renderArea.offset = { 0,0 };
		renderPassInfo.renderArea.extent = hveSwapChain->getSwapChainExtent();

		std::array<VkClearValue, 2> clearValues{};
		clearValues[0].color = { 0.01f, 0.01f, 0.01f, 1.0f };
		clearValues[1].depthStencil = { 1.0f, 0 };
		renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
		renderPassInfo.pClearValues = clearValues.data();

		vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

		VkViewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = static_cast<float>(hveSwapChain->getSwapChainExtent().width);
		viewport.height = static_cast<float>(hveSwapChain->getSwapChainExtent().height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		VkRect2D scissor = { {0,0}, hveSwapChain->getSwapChainExtent() };
		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
		vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
	}

	void HVERenderer::endSwapChainRenderPass(VkCommandBuffer commandBuffer)
	{
		assert(isFrameStarted && "Can't call endSwapChainRenderPass if frame is not in progress");
		assert(commandBuffer == getCurrentCommandBuffer() && "Can't end render pass on command buffer from a different frame");

		vkCmdEndRenderPass(commandBuffer);
	}









}


