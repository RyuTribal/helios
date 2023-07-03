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
#include <future>
#include <iostream>

#include "graphics/HVEFrameInfo.hpp"
#include "systems/HVECameraControlSystem.hpp"
#include "systems/HVEPostRenderCleanup.hpp"
#include "systems/HVEPrepareRender.hpp"


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
		std::shared_ptr<HVEPrepareRender> prepareRender = std::make_shared<HVEPrepareRender>();
		std::shared_ptr<HVEPostRenderCleanup> renderCleanup = std::make_shared<HVEPostRenderCleanup>();
		scene.addSystem(cameraController, HVESystemStages::PreRender);
		scene.addSystem(prepareRender, HVESystemStages::PreRender);
		scene.addSystem(renderSystem, HVESystemStages::DuringRender);
		// scene.addSystem(renderCleanup, HVESystemStages::PostRender);
		int cameraEntity = scene.createEntity();

		HVETransformComponent cameraTransform{};
		HVECameraComponent cameraComponent(1.f);
		cameraComponent.camera.setOrthographicProjection(0, WIDTH, HEIGHT, 0, -1, 1);
		scene.addComponentToEntity(cameraEntity, cameraTransform);
		scene.addComponentToEntity(cameraEntity, cameraComponent);

		scene.setPlayerControlledCamera(cameraEntity);

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

	static void loadObject(HVEScene *scene, int j, int i)
	{
		float tileSize = 128.f;
		auto aspect = static_cast<float>(Helios::HEIGHT) / Helios::WIDTH;

		float width = tileSize / (Helios::WIDTH * aspect);

		float height = tileSize / Helios::HEIGHT;

		int objectId = scene->createEntity();

		HVETransformComponent objectTransform;
		// The offset so they don't stack on each other
		objectTransform.translation = { tileSize * j / (Helios::WIDTH * aspect), tileSize * i / Helios::HEIGHT, 0.f };
		objectTransform.scale = { width, height, 1.f };

		scene->addComponentToEntity<HVETransformComponent>(objectId, objectTransform);

		HVETexture* texture = scene->getTextureManager()->loadTexture("assets/textures/grass.png", VK_SAMPLER_ADDRESS_MODE_REPEAT);

		HVESpriteComponent spriteComponent{ texture, {0.f, 0.f}, {16.f, 16.f} };
		scene->addComponentToEntity<HVESpriteComponent>(objectId, spriteComponent);

		scene->getModelManager()->createModel(Shapes::Quad);
		HVEModelComponent objectModel;
		objectModel.model = Shapes::Quad;
		scene->addComponentToEntity(objectId, std::move(objectModel));

		if (j == 0) {
			HVECameraComponent cameraComponent(1.f);
			cameraComponent.camera.setOrthographicProjection(0, Helios::WIDTH, Helios::HEIGHT, 0, -1, 1);
			scene->addComponentToEntity(objectId, cameraComponent);
			// scene.setPlayerControlledCamera(objectId);
		}
	}

	static void loadBatch(HVEScene* scene, int startX, int startY, int width, int height)
	{
		for (int i = startY; i < startY + height; i++)
		{
			for (int j = startX; j < startX + width; j++)
			{
				loadObject(scene, j, i);  // existing function to load a single object
			}
		}
	}

	void Helios::loadGameObjects()
	{
		int gridHeight = 2048;
		int gridWidth = 2048;
		int batchSize = 64;  // size of each batch

		for (int i = 0; i < gridHeight; i += batchSize)
		{
			for (int j = 0; j < gridWidth; j += batchSize)
			{
				futures.push_back(std::async(std::launch::async, loadBatch, &scene, j, i, batchSize, batchSize));
			}
		}

		scene.getTextureManager()->createTextureDescriptorSets();
	}

	
}
