#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace helios {

struct ScriptSoundRegistry {
    struct Entry {
        std::string name;
        std::vector<uint8_t> data;
    };

    std::unordered_map<uint32_t, Entry> sounds;
    std::unordered_map<std::string, uint32_t> name_to_id;
    uint32_t next_id = 1;

    uint32_t add(const std::string& name, std::vector<uint8_t> wav_data) {
        uint32_t id = next_id++;
        name_to_id[name] = id;
        sounds[id] = Entry{name, std::move(wav_data)};
        return id;
    }

    uint32_t find(const std::string& name) const {
        auto it = name_to_id.find(name);
        return it != name_to_id.end() ? it->second : 0;
    }
};

} // namespace helios
