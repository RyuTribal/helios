#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <type_traits>

namespace helios {

/// Read-only cursor over a contiguous byte buffer.
///
/// Reads trivially-copyable scalars, length-prefixed strings, and raw byte
/// spans, advancing an internal offset after each call.  Returns
/// zero-initialized / empty values on buffer overrun instead of throwing.
class BinaryReader {
public:
    /// Construct a reader over an external buffer (non-owning).
    BinaryReader(const uint8_t* data, size_t size)
        : m_data(data), m_size(size) {}

    /// Read a trivially-copyable value and advance the cursor.
    /// Returns a zero-initialized value on overrun.
    template <typename T>
    T read() {
        static_assert(std::is_trivially_copyable_v<T>,
                      "BinaryReader::read<T> requires a trivially copyable type");
        T value{};
        read_bytes(reinterpret_cast<uint8_t*>(&value), sizeof(T));
        return value;
    }

    /// Read a length-prefixed string (uint32_t length, then chars).
    /// Returns an empty string on overrun.
    std::string read_string() {
        auto len = read<uint32_t>();
        if (m_offset + len > m_size) {
            m_offset = m_size;
            return "";
        }
        std::string result(reinterpret_cast<const char*>(m_data + m_offset), len);
        m_offset += len;
        return result;
    }

    /// Read `count` raw bytes into `dest`.
    /// Returns false on overrun (dest is not modified and cursor is clamped).
    bool read_bytes(uint8_t* dest, size_t count) {
        if (m_offset + count > m_size) {
            m_offset = m_size;
            return false;
        }
        std::memcpy(dest, m_data + m_offset, count);
        m_offset += count;
        return true;
    }

    /// Skip `count` bytes without reading them.
    /// Returns false on overrun (cursor is clamped to end).
    bool skip(size_t count) {
        if (m_offset + count > m_size) {
            m_offset = m_size;
            return false;
        }
        m_offset += count;
        return true;
    }

    /// Current read offset in bytes.
    size_t offset() const { return m_offset; }

    /// Total buffer size in bytes.
    size_t size() const { return m_size; }

    /// Bytes remaining after the cursor.
    size_t remaining() const { return m_size - m_offset; }

    /// True when the cursor has reached the end of the buffer.
    bool at_end() const { return m_offset >= m_size; }

    /// Pointer to current position (for zero-copy reads).
    const uint8_t* current() const { return m_data + m_offset; }

private:
    const uint8_t* m_data = nullptr;
    size_t m_size = 0;
    size_t m_offset = 0;
};

} // namespace helios
