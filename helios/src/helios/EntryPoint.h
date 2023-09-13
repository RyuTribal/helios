#pragma once

#ifdef HVE_PLATFORM_WINDOWS

extern hve::Application* hve::CreateApplication();

int main(int argc, char** argv)
{
	hve::Log::Init();
	HVE_CORE_WARN("Log Initialized");
	auto app = hve::CreateApplication();
	app->run();
	delete app;
}

#else
#error Helios currently only supports Windows :(

#endif
