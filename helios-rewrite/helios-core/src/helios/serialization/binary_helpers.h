#pragma once

#include "helios/ecs/asset_handle.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cstdint>
#include <cstring>
#include <istream>
#include <ostream>
#include <string>

namespace helios {

// ============================================================
// Binary write helpers
// ============================================================

inline void write_binary(std::ostream& out, float v) {
    out.write(reinterpret_cast<const char*>(&v), sizeof(v));
}
inline void write_binary(std::ostream& out, double v) {
    out.write(reinterpret_cast<const char*>(&v), sizeof(v));
}
inline void write_binary(std::ostream& out, int v) {
    out.write(reinterpret_cast<const char*>(&v), sizeof(v));
}
inline void write_binary(std::ostream& out, uint32_t v) {
    out.write(reinterpret_cast<const char*>(&v), sizeof(v));
}
inline void write_binary(std::ostream& out, uint64_t v) {
    out.write(reinterpret_cast<const char*>(&v), sizeof(v));
}
inline void write_binary(std::ostream& out, bool v) {
    out.write(reinterpret_cast<const char*>(&v), sizeof(v));
}

inline void write_binary(std::ostream& out, const std::string& v) {
    uint32_t len = static_cast<uint32_t>(v.size());
    out.write(reinterpret_cast<const char*>(&len), sizeof(len));
    if (len > 0) {
        out.write(v.data(), len);
    }
}

inline void write_binary(std::ostream& out, const AssetHandle& v) {
    out.write(reinterpret_cast<const char*>(&v.id), sizeof(v.id));
}

inline void write_binary(std::ostream& out, const glm::vec2& v) {
    out.write(reinterpret_cast<const char*>(&v.x), sizeof(float));
    out.write(reinterpret_cast<const char*>(&v.y), sizeof(float));
}

inline void write_binary(std::ostream& out, const glm::vec3& v) {
    out.write(reinterpret_cast<const char*>(&v.x), sizeof(float));
    out.write(reinterpret_cast<const char*>(&v.y), sizeof(float));
    out.write(reinterpret_cast<const char*>(&v.z), sizeof(float));
}

inline void write_binary(std::ostream& out, const glm::vec4& v) {
    out.write(reinterpret_cast<const char*>(&v.x), sizeof(float));
    out.write(reinterpret_cast<const char*>(&v.y), sizeof(float));
    out.write(reinterpret_cast<const char*>(&v.z), sizeof(float));
    out.write(reinterpret_cast<const char*>(&v.w), sizeof(float));
}

inline void write_binary(std::ostream& out, const glm::quat& v) {
    out.write(reinterpret_cast<const char*>(&v.w), sizeof(float));
    out.write(reinterpret_cast<const char*>(&v.x), sizeof(float));
    out.write(reinterpret_cast<const char*>(&v.y), sizeof(float));
    out.write(reinterpret_cast<const char*>(&v.z), sizeof(float));
}

inline void write_binary(std::ostream& out, const glm::mat4& v) {
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            float f = v[col][row];
            out.write(reinterpret_cast<const char*>(&f), sizeof(float));
        }
    }
}

template<typename T>
    requires std::is_enum_v<T>
void write_binary(std::ostream& out, const T& v) {
    auto underlying = static_cast<std::underlying_type_t<T>>(v);
    out.write(reinterpret_cast<const char*>(&underlying), sizeof(underlying));
}

// ============================================================
// Binary read helpers
// ============================================================

inline void read_binary(std::istream& in, float& v) {
    in.read(reinterpret_cast<char*>(&v), sizeof(v));
}
inline void read_binary(std::istream& in, double& v) {
    in.read(reinterpret_cast<char*>(&v), sizeof(v));
}
inline void read_binary(std::istream& in, int& v) {
    in.read(reinterpret_cast<char*>(&v), sizeof(v));
}
inline void read_binary(std::istream& in, uint32_t& v) {
    in.read(reinterpret_cast<char*>(&v), sizeof(v));
}
inline void read_binary(std::istream& in, uint64_t& v) {
    in.read(reinterpret_cast<char*>(&v), sizeof(v));
}
inline void read_binary(std::istream& in, bool& v) {
    in.read(reinterpret_cast<char*>(&v), sizeof(v));
}

inline void read_binary(std::istream& in, std::string& v) {
    uint32_t len = 0;
    in.read(reinterpret_cast<char*>(&len), sizeof(len));
    v.resize(len);
    if (len > 0) {
        in.read(v.data(), len);
    }
}

inline void read_binary(std::istream& in, AssetHandle& v) {
    in.read(reinterpret_cast<char*>(&v.id), sizeof(v.id));
}

inline void read_binary(std::istream& in, glm::vec2& v) {
    in.read(reinterpret_cast<char*>(&v.x), sizeof(float));
    in.read(reinterpret_cast<char*>(&v.y), sizeof(float));
}

inline void read_binary(std::istream& in, glm::vec3& v) {
    in.read(reinterpret_cast<char*>(&v.x), sizeof(float));
    in.read(reinterpret_cast<char*>(&v.y), sizeof(float));
    in.read(reinterpret_cast<char*>(&v.z), sizeof(float));
}

inline void read_binary(std::istream& in, glm::vec4& v) {
    in.read(reinterpret_cast<char*>(&v.x), sizeof(float));
    in.read(reinterpret_cast<char*>(&v.y), sizeof(float));
    in.read(reinterpret_cast<char*>(&v.z), sizeof(float));
    in.read(reinterpret_cast<char*>(&v.w), sizeof(float));
}

inline void read_binary(std::istream& in, glm::quat& v) {
    in.read(reinterpret_cast<char*>(&v.w), sizeof(float));
    in.read(reinterpret_cast<char*>(&v.x), sizeof(float));
    in.read(reinterpret_cast<char*>(&v.y), sizeof(float));
    in.read(reinterpret_cast<char*>(&v.z), sizeof(float));
}

inline void read_binary(std::istream& in, glm::mat4& v) {
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            in.read(reinterpret_cast<char*>(&v[col][row]), sizeof(float));
        }
    }
}

template<typename T>
    requires std::is_enum_v<T>
void read_binary(std::istream& in, T& v) {
    std::underlying_type_t<T> underlying;
    in.read(reinterpret_cast<char*>(&underlying), sizeof(underlying));
    v = static_cast<T>(underlying);
}

} // namespace helios
