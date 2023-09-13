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

		bool wasWindowResized() { return frameBufferResized; };
		void resetWindowResizedFlag() { frameBufferResized = false; };

		void createWindowSurface(VkInstance instance, VkSurfaceKHR* surface);

		GLFWwindow* getGLFWwindow() const { return window; }
		

	private:

		static void framebufferResizeCallback(GLFWwindow* window, int width, int height);

		void initWindow();

		int WIDTH;
		int HEIGHT;

		bool frameBufferResized = false;

		std::string windowName;

		GLFWwindow* window;
	};
}

