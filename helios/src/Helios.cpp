#include "Helios.hpp"
#include "graphics/RenderSystem.hpp"
#include "graphics/HVECamera.hpp"
#include "input/HVEKeyboard.hpp"
#define GML_FORCE_RADIANS
#define GML_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>


#include <chrono>


#include <stdexcept>

namespace hve
{

	Helios::Helios()
	{
		loadGameObjects();
	}

	Helios::~Helios()
	{

	}



	void Helios::run()
	{
		RenderSystem renderSystem{ hveDevice, hveRenderer.getSwapChainRenderPass() };
		HVECamera camera{};

		auto viewerObject = HVEGameObject::createGameObject();
		KeyboardController cameraController{};

		auto currentTime = std::chrono::high_resolution_clock::now();

		while (!hveWindow.shouldClose())
		{
			glfwPollEvents();

			auto newTime = std::chrono::high_resolution_clock::now();

			float frameTime = std::chrono::duration<float, std::chrono::seconds::period>(newTime - currentTime).count();
			currentTime = newTime;

			frameTime = glm::min(frameTime, MAX_FRAME_TIME);

			cameraController.moveInPlaneXZ(hveWindow.getGLFWwindow(), frameTime, viewerObject);
			camera.setViewYXZ(viewerObject.transform.translation, viewerObject.transform.rotation);

			float aspect = hveRenderer.getAspectRatio();

			camera.setOrthographicProjection(-aspect, aspect, -1, 1, -1, 1);

			// Can be used if I want to extend the engine to be used more for 3D
			// camera.setPerspectiveProjection(glm::radians(50.f), aspect, 0.1f, 10.f);

			if (auto commandBuffer = hveRenderer.beginFrame())
			{
				hveRenderer.beingSwapChainRenderPass(commandBuffer);
				renderSystem.renderGameObjects(commandBuffer, gameObjects, camera);
				hveRenderer.endSwapChainRenderPass(commandBuffer);
				hveRenderer.endFrame();
			}
		}

		vkDeviceWaitIdle(hveDevice.device());
	}

    std::unique_ptr<HVEModel> createQuad(HVEDevice& device, glm::vec3 offset) {
		HVEModel::Builder modelBuilder{};
		modelBuilder.vertices = {
			{{-0.5f, -0.5f, .0f}, {1.0f, 0.0f, 0.0f}}, // Bottom-left corner (Red)
			{{0.5f, -0.5f, .0f}, {0.0f, 1.0f, 0.0f}},  // Bottom-right corner (Green)
			{{0.5f, 0.5f, .0f}, {0.0f, 0.0f, 1.0f}},   // Top-right corner (Blue)
			{{-0.5f, 0.5f, .0f}, {1.0f, 1.0f, 0.0f}}   // Top-left corner (Yellow)
		};

		modelBuilder.indices = {
			0, 1, 2, // First triangle (bottom-left, bottom-right, top-right)
			2, 3, 0  // Second triangle (top-right, top-left, bottom-left)
		};
        for (auto& v : modelBuilder.vertices) {
            v.position += offset;
        }
        return std::make_unique<HVEModel>(device, modelBuilder);
    }

	void Helios::loadGameObjects()
	{

		std::shared_ptr<HVEModel> hveModel = createQuad(hveDevice, { .0f, .0f, .0f });

		auto quad = HVEGameObject::createGameObject();

		quad.model = hveModel;
		quad.transform.translation = { .0f, .0f, 0.f };
		quad.transform.scale = { 1.f, 1.f, 1.f };

		gameObjects.push_back(std::move(quad));
	}





}
