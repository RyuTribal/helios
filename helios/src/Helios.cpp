#include "Helios.hpp"
#include "graphics/RenderSystem.hpp"
#include "graphics/HVECamera.hpp"
#include "input/HVEKeyboard.hpp"
#include "graphics/HVEBuffer.hpp"
#include "objects/HVETile.hpp"
#include "primitives/HVEShapes.hpp"


#define GML_FORCE_RADIANS
#define GML_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <chrono>
#include <iostream>


namespace hve
{

	struct GlobalUbo
	{
		glm::mat4 model;
		glm::mat4 view;
		glm::mat4 proj;
	};

	Helios::Helios()
	{
		globalPool = HVEDescriptorPool::Builder(hveDevice)
			.setMaxSets(HVESwapChain::MAX_FRAMES_IN_FLIGHT)
			.addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, HVESwapChain::MAX_FRAMES_IN_FLIGHT)
			.build();
		loadGameTextures();
		loadGameObjects();
	}

	Helios::~Helios()
	{
	}



	void Helios::run()
	{
		std::vector<std::unique_ptr<HVEBuffer>> uboBuffers(HVESwapChain::MAX_FRAMES_IN_FLIGHT);
		for(int i = 0; i < uboBuffers.size(); i++)
		{
			uboBuffers[i] = std::make_unique <HVEBuffer>(
				hveDevice,
				sizeof(GlobalUbo),
				1,
				VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
			);

			uboBuffers[i]->map();
		}

		auto globalSetLayout = HVEDescriptorSetLayout::Builder(hveDevice)
			.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
			.build();

		std::vector<VkDescriptorSet> globalDescriptorSets(HVESwapChain::MAX_FRAMES_IN_FLIGHT);
		for(int i = 0; i < globalDescriptorSets.size(); i++)
		{
			auto bufferInfo = uboBuffers[i]->descriptorInfo();
			HVEDescriptorWriter(*globalSetLayout, *globalPool)
				.writeBuffer(0, &bufferInfo)
				.build(globalDescriptorSets[i]);
		}

		RenderSystem renderSystem{ hveDevice, hveRenderer.getSwapChainRenderPass(), globalSetLayout->getDescriptorSetLayout(), textureManager.getDescriptionLayout()->getDescriptorSetLayout()};
		HVECamera camera{};

		auto viewerObject = HVEGameObject::createGameObject();
		KeyboardController cameraController{};

		camera.setOrthographicProjection(0, WIDTH, HEIGHT, 0, -1, 1);

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

			// Can be used if I want to extend the engine to be used more for 3D:
			// camera.setPerspectiveProjection(glm::radians(50.f), aspect, 0.1f, 10.f);

			if (auto commandBuffer = hveRenderer.beginFrame())
			{
				int frameIndex = hveRenderer.getFrameIndex();
				FrameInfo frameInfo{
					frameIndex,
					frameTime,
					commandBuffer,
					camera,
					globalDescriptorSets[frameIndex]
				};
				// update
				GlobalUbo ubo{};
				ubo.view = camera.getProjection() * camera.getView();
				uboBuffers[frameIndex]->writeToBuffer(&ubo);
				uboBuffers[frameIndex]->flush();

				//render
				hveRenderer.beingSwapChainRenderPass(commandBuffer);
				renderSystem.renderGameObjects(frameInfo, gameObjects, textureManager);
				hveRenderer.endSwapChainRenderPass(commandBuffer);
				hveRenderer.endFrame();
			}
		}

		vkDeviceWaitIdle(hveDevice.device());
	}

	void Helios::loadGameTextures()
	{
		textureManager.loadTexture("assets/textures/20jhmr.jpg");
		//textureManager.loadTexture("assets/textures/grass.png");
		textureManager.createTextureDescriptorSets();
	}

	void Helios::loadGameObjects()
	{
		int gridHeight = 64;
		int gridWidth = 64;

		float tileSize = 128.f;

		std::shared_ptr <HVEModel> quadShape = HVEShapes::drawQuad(hveDevice, { 0.f, 0.f, 0.f });


		for(int i = 0; i < gridHeight; i++)
		{
			for(int j = 0; j < gridWidth; j++)
			{
				auto aspect = static_cast<float>(HEIGHT) / WIDTH;

				float width = tileSize / (WIDTH * aspect);

				float height = tileSize / HEIGHT;

				auto quad = HVETile(quadShape, width, height, { tileSize*j/ (WIDTH * aspect), tileSize*i/HEIGHT, 0.f });

				
				gameObjects.push_back(std::move(quad));
			}
		}
	}





}
