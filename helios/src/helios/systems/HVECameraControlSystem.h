#pragma once
#include <GLFW/glfw3.h>

#include "HVESystem.h"
#include "helios/world/HVEScene.h"

namespace hve
{
	class HVECameraControlSystem : public HVESystem
	{
	public:
		struct KeyMappings
		{
			int moveLeft = GLFW_KEY_A;
			int moveRight = GLFW_KEY_D;
			int moveForward = GLFW_KEY_E;
			int moveBackward = GLFW_KEY_Q;
			int moveUp = GLFW_KEY_W;
			int moveDown = GLFW_KEY_S;
			int lookLeft = GLFW_KEY_LEFT;
			int lookRight = GLFW_KEY_RIGHT;
			int lookUp = GLFW_KEY_UP;
			int lookDown = GLFW_KEY_DOWN;
		};

		void onUpdate(FrameInfo& frameInfo, HVEScene& scene) override;

	private:
		KeyMappings keys{};
		float moveSpeed{ 3.f };
		float lookSpeed{ 1.5f };

	};
}
