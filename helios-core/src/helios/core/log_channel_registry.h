#pragma once

#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace helios {

// Forward-declare; full definition lives in log_channel.h.
struct LogChannel;

/// Central registry of all LogChannel instances.
///
/// Every LogChannel registers itself on construction and deregisters on
/// destruction. If a **different** LogChannel object tries to register a name
/// that is already taken, the process aborts with a clear diagnostic. This
/// catches accidental duplicate HELIOS_DEFINE_LOG_CHANNEL(Name) definitions
/// at startup rather than letting them silently coexist.
///
/// The same object registering twice (which cannot happen in normal code) is
/// also rejected. Inline globals that appear in multiple TUs are fine -- the
/// linker collapses them into a single object, so the constructor runs once.
///
/// All public methods are thread-safe (mutex-protected).
class LogChannelRegistry {
public:
    /// Register a channel. Aborts if a different channel with the same name
    /// is already registered.
    static void register_channel(LogChannel& channel);

    /// Remove a channel from the registry. Called by ~LogChannel().
    /// No-op if the channel was not registered (defensive).
    static void deregister_channel(LogChannel& channel);

    /// Find a channel by name. Returns nullptr if not found.
    static LogChannel* find(const std::string& name);

    /// Return all currently registered channels (snapshot under lock).
    static std::vector<LogChannel*> all();

private:
    // Meyers singletons -- safe for static-init-order and destruction.
    static std::mutex& mutex();
    static std::vector<LogChannel*>& channels();
    static std::unordered_map<std::string, LogChannel*>& name_map();
};

} // namespace helios
