#include "helios/core/uuid.h"
#include "helios/core/engine_log_channels.h"

#include <random>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cctype>

namespace helios {

const UUID UUID::NIL = {0, 0};

UUID UUID::generate() {
    // Thread-local RNG — no mutex needed, each thread has its own engine.
    static thread_local std::mt19937_64 rng{std::random_device{}()};
    static thread_local std::uniform_int_distribution<uint64_t> dist;

    UUID uuid;
    uuid.high = dist(rng);
    uuid.low = dist(rng);

    // Set version 4 (random) bits: high[bits 48-51] = 0100
    uuid.high = (uuid.high & ~(uint64_t{0xF} << 48)) | (uint64_t{0x4} << 48);

    // Set variant bits: low[bits 62-63] = 10
    uuid.low = (uuid.low & ~(uint64_t{0x3} << 62)) | (uint64_t{0x2} << 62);

    return uuid;
}

std::string UUID::to_string() const {
    // Format: xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
    // high provides first 16 hex chars, low provides last 16
    char buf[37]; // 32 hex + 4 dashes + null
    std::snprintf(buf, sizeof(buf),
        "%08x-%04x-%04x-%04x-%012llx",
        static_cast<uint32_t>(high >> 32),
        static_cast<uint16_t>(high >> 16),
        static_cast<uint16_t>(high),
        static_cast<uint16_t>(low >> 48),
        static_cast<unsigned long long>(low & 0x0000FFFFFFFFFFFF));
    return std::string(buf);
}

UUID UUID::from_string(std::string_view str) {
    // Strip dashes
    std::string hex;
    hex.reserve(32);
    for (char c : str) {
        if (c != '-') {
            if (!std::isxdigit(static_cast<unsigned char>(c))) {
                HELIOS_LOG(Assets, Error, "UUID::from_string: invalid character in '{}'", std::string(str));
                return NIL;
            }
            hex += c;
        }
    }

    if (hex.size() != 32) {
        HELIOS_LOG(Assets, Error, "UUID::from_string: expected 32 hex digits, got {} in '{}'", hex.size(), std::string(str));
        return NIL;
    }

    UUID uuid;
    uuid.high = std::stoull(hex.substr(0, 16), nullptr, 16);
    uuid.low = std::stoull(hex.substr(16, 16), nullptr, 16);
    return uuid;
}

std::ostream& operator<<(std::ostream& os, const UUID& uuid) {
    return os << uuid.to_string();
}

} // namespace helios
