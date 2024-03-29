#pragma once
#include "Core.h"
#include "Layer.h"
#include "LayerStack.h"
#include "Window.h"
#include "events/ApplicationEvent.h"
#include "ImGui/ImGuiLayer.h"


namespace Helios {
	class Application
	{
	public:
		Application();
		virtual ~Application();

		void run();

		void OnEvent(Event& event);

		void PushLayer(Layer* layer);
		void PushOverlay(Layer* overlay);

		static Application& Get() { return *s_Instance; }
		Window& GetWindow() { return *m_Window; }

	private:
		bool OnWindowClosed(WindowCloseEvent& e);

		std::unique_ptr<Window> m_Window;
		ImGuiLayer* m_ImGuiLayer;
		bool m_Running = true;
		LayerStack m_LayerStack;
	private:
		static Application* s_Instance;
	};

	Application* CreateApplication();
}

