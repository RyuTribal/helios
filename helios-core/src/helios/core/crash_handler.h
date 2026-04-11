#pragma once

namespace helios {

/// Install signal handlers for SIGSEGV, SIGABRT, SIGFPE, SIGBUS.
/// On crash: dumps ring buffer to logs/crash_context.log, then re-raises.
/// Call once at startup (LogSystem constructor does this automatically).
void install_crash_handlers();

/// Remove crash handlers (restore defaults).
void remove_crash_handlers();

} // namespace helios
