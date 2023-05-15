#pragma once

#include "graphics/HVEWindow.hpp"
#include "graphics/HVEDevice.hpp"
#include "graphics/HVERenderer.hpp"
#include "graphics/HVEGameObject.hpp"
#include "graphics/HVEDescriptors.hpp"
#include "textures/HVETextureManager.hpp"

#include <memory>
#include <vector>

#include "world/HVEScene.hpp"


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
		void loadGameObjects();

		static constexpr float MAX_FRAME_TIME = 2.f;

		HVEWindow hveWindow{WIDTH, HEIGHT, "Helios"};
		HVEDevice hveDevice{ hveWindow };
		HVERenderer hveRenderer{ hveWindow, hveDevice };
		
		std::unique_ptr<HVEDescriptorPool> globalPool{};
		HVEScene scene;
		
	};
}

