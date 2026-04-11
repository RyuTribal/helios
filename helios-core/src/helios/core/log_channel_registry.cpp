#include "helios/core/log_channel_registry.h"
#include "helios/core/log_channel.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace helios {

std::mutex& LogChannelRegistry::mutex() {
    static std::mutex m;
    return m;
}

std::vector<LogChannel*>& LogChannelRegistry::channels() {
    static std::vector<LogChannel*> v;
    return v;
}

std::unordered_map<std::string, LogChannel*>& LogChannelRegistry::name_map() {
    static std::unordered_map<std::string, LogChannel*> m;
    return m;
}

void LogChannelRegistry::register_channel(LogChannel& channel) {
    std::lock_guard lock(mutex());

    auto& map = name_map();
    auto it = map.find(channel.name);

    if (it != map.end()) {
        // Same object re-registering (shouldn't happen, but tolerate for inline ODR).
        if (it->second == &channel) {
            return;
        }

        // Different object, same name -- programmer error.
        std::cerr << "FATAL: Duplicate log channel name \""
                  << channel.name
                  << "\" -- channel names must be unique. "
                     "Check for conflicting HELIOS_DEFINE_LOG_CHANNEL("
                  << channel.name << ") definitions.\n";
        std::abort();
    }

    map.emplace(channel.name, &channel);
    channels().push_back(&channel);
}

void LogChannelRegistry::deregister_channel(LogChannel& channel) {
    std::lock_guard lock(mutex());

    auto& map = name_map();
    auto it = map.find(channel.name);
    if (it != map.end() && it->second == &channel) {
        map.erase(it);
    }

    auto& vec = channels();
    vec.erase(std::remove(vec.begin(), vec.end(), &channel), vec.end());
}

LogChannel* LogChannelRegistry::find(const std::string& name) {
    std::lock_guard lock(mutex());

    auto& map = name_map();
    auto it = map.find(name);
    if (it != map.end()) {
        return it->second;
    }
    return nullptr;
}

std::vector<LogChannel*> LogChannelRegistry::all() {
    std::lock_guard lock(mutex());
    return channels(); // returns a copy (snapshot)
}

} // namespace helios
