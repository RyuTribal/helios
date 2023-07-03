#pragma once

#ifdef HVE_PLATFORM_WINDOWS
	#ifdef HVE_BUILD_DLL
		#define HELIOS_API __declspec(dllexport)
	#else
		#define HELIOS_API __declspec(dllimport)
	#endif
#else
	#error Helios currently only supports Windows :(
#endif