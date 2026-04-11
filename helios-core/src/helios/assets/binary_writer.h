#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace helios {

/// Append-only binary serialization writer.
///
/// Builds an internal byte buffer by appending trivially-copyable scalars,
/// length-prefixed strings, and raw byte spans.  When finished, call
/// `data()` / `size()` or `take()` to retrieve the result.
class BinaryWriter {
public:
    BinaryWriter() = default;

    /// Reserve `n` bytes of capacity in the internal buffer.
    void reserve(size_t n) { m_buffer.reserve(n); }

    /// Write a trivially-copyable value (little-endian on LE hosts).
    template <typename T>
    void write(const T& value) {
        static_assert(std::is_trivially_copyable_v<T>,
                      "BinaryWriter::write<T> requires a trivially copyable type");
        const auto* bytes = reinterpret_cast<const uint8_t*>(&value);
        m_buffer.insert(m_buffer.end(), bytes, bytes + sizeof(T));
    }

    /// Write a length-prefixed string (uint32_t length, then chars).
    void write_string(std::string_view str) {
        write<uint32_t>(static_cast<uint32_t>(str.size()));
        m_buffer.insert(m_buffer.end(),
                        reinterpret_cast<const uint8_t*>(str.data()),
                        reinterpret_cast<const uint8_t*>(str.data()) + str.size());
    }

    /// Write raw bytes.
    void write_bytes(const uint8_t* data, size_t size) {
        m_buffer.insert(m_buffer.end(), data, data + size);
    }

    /// Write raw bytes from a vector.
    void write_bytes(const std::vector<uint8_t>& bytes) {
        m_buffer.insert(m_buffer.end(), bytes.begin(), bytes.end());
    }

    /// Pointer to the start of the buffer.
    const uint8_t* data() const { return m_buffer.data(); }

    /// Current size of the buffer in bytes.
    size_t size() const { return m_buffer.size(); }

    /// Move the internal buffer out (invalidates this writer).
    std::vector<uint8_t> take() { return std::move(m_buffer); }

private:
    std::vector<uint8_t> m_buffer;
};

} // namespace helios
