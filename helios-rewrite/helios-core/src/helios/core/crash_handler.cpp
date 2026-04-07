#include "helios/core/crash_handler.h"
#include "helios/core/log_system.h"
#include <csignal>
#include <cstdio>
#include <cstdlib>

namespace helios {

namespace {

// Saved previous handlers so we can restore them
struct SavedHandlers {
    struct sigaction prev_segv{};
    struct sigaction prev_abrt{};
    struct sigaction prev_fpe{};
    struct sigaction prev_bus{};
    bool installed = false;
};

SavedHandlers g_saved_handlers;

const char* signal_name(int sig) {
    switch (sig) {
        case SIGSEGV: return "SIGSEGV (Segmentation fault)";
        case SIGABRT: return "SIGABRT (Abort)";
        case SIGFPE:  return "SIGFPE (Floating-point exception)";
        case SIGBUS:  return "SIGBUS (Bus error)";
        default:      return "Unknown signal";
    }
}

void crash_signal_handler(int sig) {
    // Print to stderr -- not strictly async-signal-safe but widely works
    // in practice for crash dumps. Better than losing the information.
    std::fprintf(stderr,
        "\n=== HELIOS CRASH ===\n"
        "  Signal: %d (%s)\n"
        "====================\n\n",
        sig, signal_name(sig));

    // Dump crash context if LogSystem is available
    auto* log = LogSystem::instance();
    if (log) {
        log->dump_crash_context();
    }

    // Restore default handler and re-raise so the OS generates a core dump
    struct sigaction sa{};
    sa.sa_handler = SIG_DFL;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(sig, &sa, nullptr);
    raise(sig);
}

} // anonymous namespace

void install_crash_handlers() {
    if (g_saved_handlers.installed) return;

    struct sigaction sa{};
    sa.sa_handler = crash_signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESETHAND; // One-shot: reset to default after first invocation

    sigaction(SIGSEGV, &sa, &g_saved_handlers.prev_segv);
    sigaction(SIGABRT, &sa, &g_saved_handlers.prev_abrt);
    sigaction(SIGFPE,  &sa, &g_saved_handlers.prev_fpe);
    sigaction(SIGBUS,  &sa, &g_saved_handlers.prev_bus);

    g_saved_handlers.installed = true;
}

void remove_crash_handlers() {
    if (!g_saved_handlers.installed) return;

    sigaction(SIGSEGV, &g_saved_handlers.prev_segv, nullptr);
    sigaction(SIGABRT, &g_saved_handlers.prev_abrt, nullptr);
    sigaction(SIGFPE,  &g_saved_handlers.prev_fpe,  nullptr);
    sigaction(SIGBUS,  &g_saved_handlers.prev_bus,  nullptr);

    g_saved_handlers.installed = false;
}

} // namespace helios
