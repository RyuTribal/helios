#pragma once

#include "HVEWindow.h"
#include "HVEPipeline.h"
#include "HVEDevice.h"
#include "HVESwapChain.h"

#include <memory>
#include <vector>

namespace hve {
	class Helios
	{

	public:
		static constexpr int WIDTH = 800;
		static constexpr int HEIGHT = 600;

		Helios();
		~Helios();

		Helios(const Helios&) = delete;
		Helios& operator=(const Helios&) = delete;

		void run();
	private:
		void createPipelineLayout();
		void createPipeline();
		void createCommandBuffers();
		void drawFrame();

		HVEWindow hveWindow{WIDTH, HEIGHT, "Helios"};
		HVEDevice hveDevice{ hveWindow };
		HVESwapChain hveSwapChain{ hveDevice, hveWindow.getExtent() };
		std::unique_ptr<HVEPipeline> hvePipeline;
		VkPipelineLayout pipelineLayout;
		std::vector<VkCommandBuffer> commandBuffers;
	};
}

