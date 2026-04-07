#pragma once
#include <vector>
#include <cstddef>
#include <cstring>
#include <cassert>
#include <functional>

namespace helios {

class Column {
public:
    using DestructorFn = std::function<void(void*)>;
    using MoveConstructFn = std::function<void(void* dst, void* src)>;

    Column() = default;

    Column(size_t element_size, size_t element_align,
           DestructorFn destructor, MoveConstructFn move_construct)
        : m_element_size(element_size)
        , m_element_align(element_align)
        , m_destructor(std::move(destructor))
        , m_move_construct(std::move(move_construct)) {}

    ~Column() { clear(); }

    Column(Column&& other) noexcept
        : m_data(std::move(other.m_data))
        , m_count(other.m_count)
        , m_element_size(other.m_element_size)
        , m_element_align(other.m_element_align)
        , m_destructor(std::move(other.m_destructor))
        , m_move_construct(std::move(other.m_move_construct)) {
        other.m_count = 0;
    }

    Column& operator=(Column&& other) noexcept {
        if (this != &other) {
            clear();
            m_data = std::move(other.m_data);
            m_count = other.m_count;
            m_element_size = other.m_element_size;
            m_element_align = other.m_element_align;
            m_destructor = std::move(other.m_destructor);
            m_move_construct = std::move(other.m_move_construct);
            other.m_count = 0;
        }
        return *this;
    }

    Column(const Column&) = delete;
    Column& operator=(const Column&) = delete;

    void push(void* src) {
        m_data.resize((m_count + 1) * m_element_size);
        void* dst = m_data.data() + m_count * m_element_size;
        m_move_construct(dst, src);
        ++m_count;
    }

    void swap_remove(size_t index) {
        assert(index < m_count);
        void* target = m_data.data() + index * m_element_size;
        m_destructor(target);

        size_t last = m_count - 1;
        if (index != last) {
            void* last_ptr = m_data.data() + last * m_element_size;
            m_move_construct(target, last_ptr);
            m_destructor(last_ptr);
        }

        m_data.resize(last * m_element_size);
        --m_count;
    }

    void move_out_and_swap_remove(size_t index, void* dst) {
        assert(index < m_count);
        void* src = m_data.data() + index * m_element_size;
        m_move_construct(dst, src);
        m_destructor(src);

        size_t last = m_count - 1;
        if (index != last) {
            void* last_ptr = m_data.data() + last * m_element_size;
            m_move_construct(src, last_ptr);
            m_destructor(last_ptr);
        }

        m_data.resize(last * m_element_size);
        --m_count;
    }

    void* get_raw(size_t index) {
        assert(index < m_count);
        return m_data.data() + index * m_element_size;
    }

    const void* get_raw(size_t index) const {
        assert(index < m_count);
        return m_data.data() + index * m_element_size;
    }

    template <typename T> T& get(size_t index) {
        return *reinterpret_cast<T*>(get_raw(index));
    }

    template <typename T> const T& get(size_t index) const {
        return *reinterpret_cast<const T*>(get_raw(index));
    }

    template <typename T> T* data() {
        return reinterpret_cast<T*>(m_data.data());
    }

    template <typename T> const T* data() const {
        return reinterpret_cast<const T*>(m_data.data());
    }

    size_t count() const { return m_count; }
    size_t element_size() const { return m_element_size; }
    bool empty() const { return m_count == 0; }

    void clear() {
        if (m_destructor) {
            for (size_t i = 0; i < m_count; ++i) {
                m_destructor(m_data.data() + i * m_element_size);
            }
        }
        m_data.clear();
        m_count = 0;
    }

    template <typename T>
    static Column create() {
        return Column(
            sizeof(T), alignof(T),
            [](void* ptr) { reinterpret_cast<T*>(ptr)->~T(); },
            [](void* dst, void* src) { new (dst) T(std::move(*reinterpret_cast<T*>(src))); }
        );
    }

private:
    std::vector<std::byte> m_data;
    size_t m_count = 0;
    size_t m_element_size = 0;
    size_t m_element_align = 0;
    DestructorFn m_destructor;
    MoveConstructFn m_move_construct;
};

} // namespace helios
