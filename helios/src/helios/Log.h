#pragma once

#include "Core.h"

// This ignores all warnings raised inside External headers
#pragma warning(push, 0)
#include "spdlog/spdlog.h"
#include "spdlog/fmt/ostr.h"
#pragma warning(pop)


namespace Helios
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
#define HVE_CORE_FATAL(...) ::Helios::Log::GetCoreLogger()->critical(__VA_ARGS__)
#define HVE_CORE_ERROR(...) ::Helios::Log::GetCoreLogger()->error(__VA_ARGS__)
#define HVE_CORE_WARN(...) ::Helios::Log::GetCoreLogger()->warn(__VA_ARGS__)
#define HVE_CORE_INFO(...) ::Helios::Log::GetCoreLogger()->info(__VA_ARGS__)
#define HVE_CORE_TRACE(...) ::Helios::Log::GetCoreLogger()->trace(__VA_ARGS__)


// Client log macros
#define HVE_FATAL(...) ::Helios::Log::GetClientLogger()->critical(__VA_ARGS__)
#define HVE_ERROR(...) ::Helios::Log::GetClientLogger()->error(__VA_ARGS__)
#define HVE_WARN(...) ::Helios::Log::GetClientLogger()->warn(__VA_ARGS__)
#define HVE_INFO(...) ::Helios::Log::GetClientLogger()->info(__VA_ARGS__)
#define HVE_TRACE(...) ::Helios::Log::GetClientLogger()->trace(__VA_ARGS__)