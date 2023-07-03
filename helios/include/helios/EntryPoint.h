#pragma once

#ifdef HVE_PLATFORM_WINDOWS

extern hve::Application* hve::CreateApplication();

int main(int argc, char** argv)
{
	hve::Log::Init();
	HVE_CORE_WARN("Welcome to helios Engine");
	auto app = hve::CreateApplication();
	app->run();
	delete app;
}

#else
#error Helios currently only supports Windows :(

#endif
