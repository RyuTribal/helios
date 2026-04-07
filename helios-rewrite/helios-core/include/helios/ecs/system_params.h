#pragma once

#include <cassert>

namespace helios {

/// Immutable (read-only) resource wrapper.
/// Wraps a const reference to a resource stored in a World/ResourceStorage.
template <typename T>
class Res {
public:
    explicit Res(const T* ptr) : m_ptr(ptr) {
        assert(ptr && "Res: null resource pointer");
    }

    const T& operator*()  const { return *m_ptr; }
    const T* operator->() const { return m_ptr; }
    const T* get()        const { return m_ptr; }

private:
    const T* m_ptr;
};

/// Mutable resource wrapper.
/// Wraps a non-const pointer to a resource stored in a World/ResourceStorage.
template <typename T>
class ResMut {
public:
    explicit ResMut(T* ptr) : m_ptr(ptr) {
        assert(ptr && "ResMut: null resource pointer");
    }

    T& operator*()  const { return *m_ptr; }
    T* operator->() const { return m_ptr; }
    T* get()        const { return m_ptr; }

private:
    T* m_ptr;
};

} // namespace helios
