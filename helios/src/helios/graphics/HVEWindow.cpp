#include "hvepch.h"
#include "helios/graphics/HVEWindow.h"




namespace hve
{
	HVEWindow::HVEWindow(int w, int h, std::string name) : WIDTH{w}, HEIGHT{h}, windowName{name}
	{
		initWindow();
	}

	HVEWindow::~HVEWindow()
	{
		glfwDestroyWindow(window);
		glfwTerminate();

	}


	void HVEWindow::initWindow()
	{
		glfwInit();
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

		window = glfwCreateWindow(WIDTH, HEIGHT, windowName.c_str(), nullptr, nullptr);
		glfwSetWindowUserPointer(window, this);
		glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
	}

	void HVEWindow::framebufferResizeCallback(GLFWwindow* window, int width, int height)
	{
		auto hveWindow = reinterpret_cast < HVEWindow* > (glfwGetWindowUserPointer(window));
		hveWindow->frameBufferResized = true;
		hveWindow->WIDTH = width;
		hveWindow->HEIGHT = height;
	}


	void HVEWindow::createWindowSurface(VkInstance instance, VkSurfaceKHR* surface)
	{
		if(glfwCreateWindowSurface(instance, window, nullptr, surface) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create window surface");
		}
	}



}
