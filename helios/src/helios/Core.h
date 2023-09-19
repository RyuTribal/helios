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

#ifdef HVE_DEBUG
	#define HVE_ENABLE_ASSERTS
#endif

#ifdef HVE_ENABLE_ASSERTS
	#define HVE_ASSERT(x, ...) { if(!(x)) { HVE_ERROR("Assertion Failed: {0}", __VA_ARGS__); __debugbreak();}}
	#define HVE_CORE_ASSERT(x, ...) { if(!(x)) { HVE_CORE_ERROR("Assertion Failed: {0}", __VA_ARGS__); __debugbreak();}}
#else
	#define HVE_ASSERT(x, ...)
	#define HVE_CORE_ASSERT(x, ...)
#endif

#define BIT(x) (1 << x)

#define HVE_BIND_EVENT_FN(fn) std::bind(&fn, this, std::placeholders::_1)