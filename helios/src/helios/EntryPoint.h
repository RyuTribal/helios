#pragma once

#ifdef HVE_PLATFORM_WINDOWS

extern Helios::Application* Helios::CreateApplication();

int main(int argc, char** argv)
{
	Helios::Log::Init();
	HVE_CORE_WARN("Log Initialized");
	auto app = Helios::CreateApplication();
	app->run();
	delete app;
}

#else
#error Helios currently only supports Windows :(

#endif
