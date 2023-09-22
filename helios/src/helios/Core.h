#pragma once


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