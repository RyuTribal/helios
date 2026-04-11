#include "helios/core/assert.h"
#include "helios/core/log_system.h"
#include <cstdio>
#include <cstdlib>

namespace helios {

[[noreturn]] void assert_fail(const char* condition, const char* file, int line,
                              const char* func, const char* msg) {
    // Always print to stderr (even if no LogSystem exists)
    if (msg) {
        std::fprintf(stderr,
            "\n=== HELIOS ASSERT FAILED ===\n"
            "  Condition: %s\n"
            "  Message:   %s\n"
            "  Location:  %s:%d\n"
            "  Function:  %s\n"
            "============================\n\n",
            condition, msg, file, line, func);
    } else {
        std::fprintf(stderr,
            "\n=== HELIOS ASSERT FAILED ===\n"
            "  Condition: %s\n"
            "  Location:  %s:%d\n"
            "  Function:  %s\n"
            "============================\n\n",
            condition, file, line, func);
    }

    // Log through the logging system if available
    auto* log = LogSystem::instance();
    if (log) {
        if (msg) {
            auto* logger = log->get_or_create_channel_logger("Core");
            if (logger) {
                logger->critical("ASSERT FAILED: {} | {} | {}:{} in {}",
                    condition, msg, file, line, func);
            }
        } else {
            auto* logger = log->get_or_create_channel_logger("Core");
            if (logger) {
                logger->critical("ASSERT FAILED: {} | {}:{} in {}",
                    condition, file, line, func);
            }
        }
        // Dump crash context (ring buffer to file)
        log->dump_crash_context();
    }

    std::abort();
}

} // namespace helios
