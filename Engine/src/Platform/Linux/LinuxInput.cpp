#include "pch.h"
#include "LinuxInput.h"
#include "Core/Application.h"

#include <GLFW/glfw3.h>

namespace Engine
{
	Input *Input::s_Instance = new LinuxInput();

	bool LinuxInput::IsKeyPressedImpl(int keycode)
	{
		auto& window = Application::Get().GetWindow();
		auto native_window = static_cast<GLFWwindow *>(window.GetNativeWindow());

		auto state = glfwGetKey(native_window, keycode);
		return window.IsKeyPressed(keycode) || state == GLFW_REPEAT;
	}

	void LinuxInput::SetLockMouseModeImpl(bool lock_mouse)
	{
		if (lock_mouse != b_IsLocked) {
			auto window = static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindow());
			glfwSetCursorPos(window, Application::Get().GetWindow().GetWidth() / 2, Application::Get().GetWindow().GetHeight() / 2);
			lock_mouse ? glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED) : glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
			b_IsLocked = lock_mouse;
		}
	}

	bool LinuxInput::IsMouseButtonPressedImpl(int button)
	{
		auto window = static_cast<GLFWwindow *>(Application::Get().GetWindow().GetNativeWindow());

		auto state = glfwGetMouseButton(window, button);
		return state == GLFW_PRESS;
	}
	float LinuxInput::GetMouseXImpl()
	{
		auto [x, y] = GetMousePositionImpl();

		return x;
	}
	float LinuxInput::GetMouseYImpl()
	{
		auto [x, y] = GetMousePositionImpl();

		return y;
	}
	std::pair<float, float> LinuxInput::GetMousePositionImpl()
	{
		auto window = static_cast<GLFWwindow *>(Application::Get().GetWindow().GetNativeWindow());

		double xpos, ypos;
		glfwGetCursorPos(window, &xpos, &ypos);

		return {(float)xpos, (float)ypos};
	}
	void LinuxInput::ClearKeyStatesImpl()
	{
		Application::Get().GetWindow().ClearKeyStates();
	}
}
