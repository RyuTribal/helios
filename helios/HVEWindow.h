#pragma once

#define GLFW_INCLUDE_VULKAN
#include <string>
#include <GLFW/glfw3.h>

namespace hve
{
	class HVEWindow
	{
	public:
		HVEWindow(int w, int h, std::string name);
		~HVEWindow();

		HVEWindow(const HVEWindow&) = delete;
		HVEWindow& operator=(const HVEWindow&) = delete;

		bool shouldClose() { return glfwWindowShouldClose(window); }

		VkExtent2D getExtent() {
			return { static_cast<uint32_t>(WIDTH), static_cast<uint32_t>(HEIGHT) };
		}

		void createWindowSurface(VkInstance instance, VkSurfaceKHR* surface);

	private:

		void initWindow();

		const int WIDTH;
		const int HEIGHT;

		std::string windowName;

		GLFWwindow* window;
	};
}

