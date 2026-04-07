#pragma once

#include <any>
#include <stdexcept>
#include <typeindex>
#include <unordered_map>

namespace helios {

class ResourceStorage {
public:
    ResourceStorage() = default;

    template <typename T>
    void insert(T resource) {
        m_resources[std::type_index(typeid(T))] = std::make_any<T>(std::move(resource));
    }

    template <typename T>
    T& get() {
        auto it = m_resources.find(std::type_index(typeid(T)));
        if (it == m_resources.end()) {
            throw std::out_of_range("ResourceStorage: resource not found");
        }
        return std::any_cast<T&>(it->second);
    }

    template <typename T>
    const T& get() const {
        auto it = m_resources.find(std::type_index(typeid(T)));
        if (it == m_resources.end()) {
            throw std::out_of_range("ResourceStorage: resource not found");
        }
        return std::any_cast<const T&>(it->second);
    }

    template <typename T>
    T* try_get() {
        auto it = m_resources.find(std::type_index(typeid(T)));
        if (it == m_resources.end()) {
            return nullptr;
        }
        return std::any_cast<T>(&it->second);
    }

    template <typename T>
    const T* try_get() const {
        auto it = m_resources.find(std::type_index(typeid(T)));
        if (it == m_resources.end()) {
            return nullptr;
        }
        return std::any_cast<const T>(&it->second);
    }

    template <typename T>
    bool has() const {
        return m_resources.count(std::type_index(typeid(T))) > 0;
    }

    template <typename T>
    void remove() {
        m_resources.erase(std::type_index(typeid(T)));
    }

    size_t count() const {
        return m_resources.size();
    }

private:
    std::unordered_map<std::type_index, std::any> m_resources;
};

} // namespace helios
