#include "hvepch.h"
#include "helios/Application.h"

#include "helios/events/Event.h"
#include "helios/Log.h"

namespace hve
{
#define BIND_EVENT_FN(x) std::bind(&Application::x, this, std::placeholders::_1) 
	Application::Application()
	{
		m_Window = std::unique_ptr<Window>(Window::Create());
		m_Window->SetEventCallback(BIND_EVENT_FN(OnEvent));
	}

	void Application::OnEvent(Event& event)
	{
		EventDispatcher dispatcher(event);
		dispatcher.Dispatch<WindowCloseEvent>(BIND_EVENT_FN(OnWindowClosed));

		HVE_CORE_TRACE("{0}", event);
	}


	Application::~Application()
	{
	}

	void Application::run()
	{
		while (m_Running)
		{
			m_Window->OnUpdate();
		}
	}

	bool Application::OnWindowClosed(WindowCloseEvent& e)
	{
		m_Running = false;
		return false;
	}
}

