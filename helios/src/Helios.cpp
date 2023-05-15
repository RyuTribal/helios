#include "Helios.hpp"
#include "systems/RenderSystem.hpp"
#include "graphics/HVECamera.hpp"
#include "input/HVEKeyboard.hpp"
#include "graphics/HVEBuffer.hpp"
#include "primitives/HVEShapes.hpp"


#define GML_FORCE_RADIANS
#define GML_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <chrono>
#include <iostream>

#include "graphics/HVEFrameInfo.hpp"
#include "systems/HVECameraControlSystem.hpp"


namespace hve
{

	struct GlobalUbo
	{
		glm::mat4 model;
		glm::mat4 view;
		glm::mat4 proj;
	};

	Helios::Helios() : scene{ HVEScene::createScene(hveWindow, hveDevice, hveRenderer) }
	{
		globalPool = HVEDescriptorPool::Builder(hveDevice)
			.setMaxSets(HVESwapChain::MAX_FRAMES_IN_FLIGHT)
			.addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, HVESwapChain::MAX_FRAMES_IN_FLIGHT)
			.build();
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

		std::shared_ptr<RenderSystem> renderSystem = std::make_shared<RenderSystem>( hveDevice, hveRenderer.getSwapChainRenderPass(), globalSetLayout->getDescriptorSetLayout(), scene.getTextureManager()->getDescriptionLayout()->getDescriptorSetLayout() );
		std::shared_ptr<HVECameraControlSystem> cameraController = std::make_shared<HVECameraControlSystem>();
		scene.addSystem(cameraController, HVESystemStages::PreRender);
		scene.addSystem(renderSystem, HVESystemStages::DuringRender);
		int cameraEntity = scene.createEntity();

		HVETransformComponent cameraTransform{};
		HVECameraComponent cameraComponent(1.f);
		cameraComponent.camera.setOrthographicProjection(0, WIDTH, HEIGHT, 0, -1, 1);
		scene.addComponentToEntity(cameraEntity, cameraTransform);
		scene.addComponentToEntity(cameraEntity, cameraComponent);

		//scene.setPlayerControlledCamera(cameraEntity);

		auto currentTime = std::chrono::high_resolution_clock::now();
		while (!hveWindow.shouldClose())
		{
			glfwPollEvents();

			auto newTime = std::chrono::high_resolution_clock::now();

			float frameTime = std::chrono::duration<float, std::chrono::seconds::period>(newTime - currentTime).count();
			currentTime = newTime;

			frameTime = glm::min(frameTime, MAX_FRAME_TIME);


			
			// Can be used if I want to extend the engine to be used more for 3D:
			// camera.setPerspectiveProjection(glm::radians(50.f), aspect, 0.1f, 10.f);

			scene.update(frameTime, globalDescriptorSets, uboBuffers);

			
		}

		vkDeviceWaitIdle(hveDevice.device());
	}

	void Helios::loadGameObjects()
	{
		int gridHeight = 2;
		int gridWidth = 2;

		float tileSize = 256.f;

		std::shared_ptr <HVEModel> quadShape = HVEShapes::drawQuad(hveDevice, { 0.f, 0.f, 0.f });
		bool hasCamera = false;
		for(int i = 0; i < gridHeight; i++)
		{
			for(int j = 0; j < gridWidth; j++)
			{
				auto aspect = static_cast<float>(HEIGHT) / WIDTH;

				float width = tileSize / (WIDTH * aspect);

				float height = tileSize / HEIGHT;

				int objectId = scene.createEntity();

				HVEModelComponent objectModel;
				objectModel.model = quadShape;

				scene.addComponentToEntity(objectId, std::move(objectModel));

				HVETransformComponent objectTransform;
				// The offset so they don't stack on each other
				objectTransform.translation = { tileSize * j / (WIDTH * aspect), tileSize * i / HEIGHT, 0.f };
				objectTransform.scale = { width, height, 1.f };

				scene.addComponentToEntity<HVETransformComponent>(objectId, objectTransform);

				HVETexture *texture = scene.getTextureManager()->loadTexture("assets/textures/20jhmr.jpg");

				HVETextureComponent objectTexture{ texture };
				scene.addComponentToEntity<HVETextureComponent>(objectId, objectTexture);

				if (!hasCamera) {
					HVECameraComponent cameraComponent(1.f);
					cameraComponent.camera.setOrthographicProjection(0, WIDTH, HEIGHT, 0, -1, 1);
					scene.addComponentToEntity(objectId, cameraComponent);
					hasCamera = true;
					scene.setPlayerControlledCamera(objectId);
				}
				
			}
		}
		scene.getTextureManager()->createTextureDescriptorSets();
	}






}
