#pragma once

#include <memory>
#include <stdexcept>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace helios {

// Abstract base for type-erased event channels
class EventChannelBase {
public:
    virtual ~EventChannelBase() = default;
    virtual void swap_buffers() = 0;
    virtual size_t read_count() const = 0;
    virtual size_t write_count() const = 0;
};

template <typename T>
class EventChannel : public EventChannelBase {
public:
    EventChannel() = default;

    void send(const T& event) {
        m_write_buffer.push_back(event);
    }

    void send(T&& event) {
        m_write_buffer.push_back(std::move(event));
    }

    // Swap: read buffer becomes the previous write buffer, write buffer is cleared
    void swap_buffers() override {
        std::swap(m_read_buffer, m_write_buffer);
        m_write_buffer.clear();
    }

    const std::vector<T>& read_buffer() const {
        return m_read_buffer;
    }

    size_t read_count() const override {
        return m_read_buffer.size();
    }

    size_t write_count() const override {
        return m_write_buffer.size();
    }

private:
    std::vector<T> m_read_buffer;
    std::vector<T> m_write_buffer;
};

template <typename T>
class EventWriter {
public:
    explicit EventWriter(EventChannel<T>* channel) : m_channel(channel) {}

    void send(const T& event) {
        m_channel->send(event);
    }

    void send(T&& event) {
        m_channel->send(std::move(event));
    }

private:
    EventChannel<T>* m_channel;
};

template <typename T>
class EventReader {
public:
    explicit EventReader(const EventChannel<T>* channel) : m_channel(channel) {}

    typename std::vector<T>::const_iterator begin() const {
        return m_channel->read_buffer().begin();
    }

    typename std::vector<T>::const_iterator end() const {
        return m_channel->read_buffer().end();
    }

    bool is_empty() const {
        return m_channel->read_buffer().empty();
    }

    size_t count() const {
        return m_channel->read_buffer().size();
    }

private:
    const EventChannel<T>* m_channel;
};

class EventStorage {
public:
    EventStorage() = default;

    template <typename T>
    void register_event() {
        m_channels[std::type_index(typeid(T))] = std::make_unique<EventChannel<T>>();
    }

    template <typename T>
    EventChannel<T>& get_channel() {
        auto it = m_channels.find(std::type_index(typeid(T)));
        if (it == m_channels.end()) {
            throw std::out_of_range("EventStorage: event channel not registered");
        }
        return static_cast<EventChannel<T>&>(*it->second);
    }

    template <typename T>
    const EventChannel<T>& get_channel() const {
        auto it = m_channels.find(std::type_index(typeid(T)));
        if (it == m_channels.end()) {
            throw std::out_of_range("EventStorage: event channel not registered");
        }
        return static_cast<const EventChannel<T>&>(*it->second);
    }

    template <typename T>
    EventWriter<T> writer() {
        return EventWriter<T>(&get_channel<T>());
    }

    template <typename T>
    EventReader<T> reader() {
        return EventReader<T>(&get_channel<T>());
    }

    template <typename T>
    bool has_channel() const {
        return m_channels.count(std::type_index(typeid(T))) > 0;
    }

    void swap_all_buffers() {
        for (auto& [key, channel] : m_channels) {
            channel->swap_buffers();
        }
    }

private:
    std::unordered_map<std::type_index, std::unique_ptr<EventChannelBase>> m_channels;
};

} // namespace helios
