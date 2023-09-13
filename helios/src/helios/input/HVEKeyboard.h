#pragma once

#include "helios/graphics/HVEGameObject.h"



#include "helios/graphics/HVEWindow.h"

namespace hve
{
	class KeyboardController
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

        void moveInPlaneXZ(GLFWwindow* window, float dt, HVEGameObject& gameObject);


        KeyMappings keys{};
        float moveSpeed{ 3.f };
        float lookSpeed{ 1.5f };
	};
}
