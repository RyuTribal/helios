#pragma once

#include <functional>
#include <stdexcept>
#include <typeindex>
#include <unordered_map>
#include <memory>

namespace helios {

// Type-erased storage slot that supports move-only types.
// Each slot owns a heap-allocated resource of a specific type.
// Unlike std::any, this does NOT require CopyConstructible.
struct ResourceSlot {
    void*                     ptr     = nullptr;   // owning raw pointer
    std::function<void(void*)> deleter = nullptr;   // type-safe destructor

    ResourceSlot() = default;

    template <typename T>
    static ResourceSlot make(T resource) {
        ResourceSlot slot;
        slot.ptr     = static_cast<void*>(new T(std::move(resource)));
        slot.deleter = [](void* p) { delete static_cast<T*>(p); };
        return slot;
    }

    // Move-only
    ResourceSlot(const ResourceSlot&) = delete;
    ResourceSlot& operator=(const ResourceSlot&) = delete;

    ResourceSlot(ResourceSlot&& other) noexcept
        : ptr(other.ptr), deleter(std::move(other.deleter))
    {
        other.ptr     = nullptr;
        other.deleter = nullptr;
    }

    ResourceSlot& operator=(ResourceSlot&& other) noexcept {
        if (this != &other) {
            destroy();
            ptr           = other.ptr;
            deleter       = std::move(other.deleter);
            other.ptr     = nullptr;
            other.deleter = nullptr;
        }
        return *this;
    }

    ~ResourceSlot() { destroy(); }

    template <typename T>
    T& as() { return *static_cast<T*>(ptr); }

    template <typename T>
    const T& as() const { return *static_cast<const T*>(ptr); }

private:
    void destroy() {
        if (ptr && deleter) {
            deleter(ptr);
            ptr     = nullptr;
            deleter = nullptr;
        }
    }
};

class ResourceStorage {
public:
    ResourceStorage() = default;

    // Move-only (slots are move-only).
    ResourceStorage(const ResourceStorage&) = delete;
    ResourceStorage& operator=(const ResourceStorage&) = delete;
    ResourceStorage(ResourceStorage&&) = default;
    ResourceStorage& operator=(ResourceStorage&&) = default;

    // Destructor: destroy resources in reverse insertion order.
    // Resources inserted later (which may depend on earlier ones)
    // are destroyed first — e.g. GPU pipelines before the device.
    ~ResourceStorage() {
        for (auto it = m_insertion_order.rbegin(); it != m_insertion_order.rend(); ++it) {
            m_resources.erase(*it);
        }
        m_insertion_order.clear();
    }

    template <typename T>
    void insert(T resource) {
        auto key = std::type_index(typeid(T));
        bool is_new = !m_resources.count(key);
        m_resources.insert_or_assign(key, ResourceSlot::make<T>(std::move(resource)));
        if (is_new) {
            m_insertion_order.push_back(key);
        }
    }

    template <typename T>
    T& get() {
        auto it = m_resources.find(std::type_index(typeid(T)));
        if (it == m_resources.end()) {
            throw std::out_of_range("ResourceStorage: resource not found");
        }
        return it->second.as<T>();
    }

    template <typename T>
    const T& get() const {
        auto it = m_resources.find(std::type_index(typeid(T)));
        if (it == m_resources.end()) {
            throw std::out_of_range("ResourceStorage: resource not found");
        }
        return it->second.as<T>();
    }

    template <typename T>
    T* try_get() {
        auto it = m_resources.find(std::type_index(typeid(T)));
        if (it == m_resources.end()) {
            return nullptr;
        }
        return static_cast<T*>(it->second.ptr);
    }

    template <typename T>
    const T* try_get() const {
        auto it = m_resources.find(std::type_index(typeid(T)));
        if (it == m_resources.end()) {
            return nullptr;
        }
        return static_cast<const T*>(it->second.ptr);
    }

    template <typename T>
    bool has() const {
        return m_resources.count(std::type_index(typeid(T))) > 0;
    }

    template <typename T>
    void remove() {
        auto key = std::type_index(typeid(T));
        m_resources.erase(key);
        std::erase(m_insertion_order, key);
    }

    size_t count() const {
        return m_resources.size();
    }

private:
    std::unordered_map<std::type_index, ResourceSlot> m_resources;
    std::vector<std::type_index> m_insertion_order;
};

} // namespace helios
