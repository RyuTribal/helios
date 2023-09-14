#pragma once
#include "Core.h"
#include "Layer.h"
#include "LayerStack.h"
#include "Window.h"
#include "events/ApplicationEvent.h"


namespace hve {
	class HELIOS_API Application
	{
	public:
		Application();
		virtual ~Application();

		void run();

		void OnEvent(Event& event);

		void PushLayer(Layer* layer);
		void PushOverlay(Layer* overlay);

	private:
		bool OnWindowClosed(WindowCloseEvent& e);

		std::unique_ptr<Window> m_Window;
		bool m_Running = true;
		LayerStack m_LayerStack;
	};

	Application* CreateApplication();
}

