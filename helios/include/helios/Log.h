#pragma once

#include "Core.h"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <memory>

namespace hve
{
	class HELIOS_API Log
	{
	public:
		static void Init();

		inline static std::shared_ptr<spdlog::logger>& GetCoreLogger() { return s_CoreLogger; }
		inline static std::shared_ptr<spdlog::logger>& GetClientLogger() { return s_ClientLogger; }

	private:
		static std::shared_ptr<spdlog::logger> s_CoreLogger;
		static std::shared_ptr<spdlog::logger> s_ClientLogger;
	};
}

// Core log macros
#define HVE_CORE_FATAL(...) ::hve::Log::GetCoreLogger()->fatal(__VA_ARGS__)
#define HVE_CORE_ERROR(...) ::hve::Log::GetCoreLogger()->error(__VA_ARGS__)
#define HVE_CORE_WARN(...) ::hve::Log::GetCoreLogger()->warn(__VA_ARGS__)
#define HVE_CORE_INFO(...) ::hve::Log::GetCoreLogger()->info(__VA_ARGS__)
#define HVE_CORE_TRACE(...) ::hve::Log::GetCoreLogger()->trace(__VA_ARGS__)


// Client log macros
#define HVE_FATAL(...) ::hve::Log::GetClientLogger()->fatal(__VA_ARGS__)
#define HVE_ERROR(...) ::hve::Log::GetClientLogger()->error(__VA_ARGS__)
#define HVE_WARN(...) ::hve::Log::GetCClientogger()->warn(__VA_ARGS__)
#define HVE_INFO(...) ::hve::Log::GetCClientogger()->info(__VA_ARGS__)
#define HVE_TRACE(...) ::hve::Log::GetClientLogger()->trace(__VA_ARGS__)