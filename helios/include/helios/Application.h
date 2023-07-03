#pragma once
#include "Core.h"

namespace hve {
	class HELIOS_API Application
	{
	public:
		Application();
		virtual ~Application();

		void run();
	};

	Application* CreateApplication();
}

