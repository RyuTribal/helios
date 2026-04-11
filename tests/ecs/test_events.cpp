#include <gtest/gtest.h>
#include <string>
#include "helios/ecs/event_storage.h"

using namespace helios;

struct DamageEvent {
    int amount = 0;
};

struct SpawnEvent {
    int entity_id = 0;
};

// ---- EventChannel tests ----

TEST(EventChannel, SendAndSwap) {
    EventChannel<DamageEvent> channel;
    channel.send(DamageEvent{10});
    channel.send(DamageEvent{20});
    EXPECT_EQ(channel.write_count(), 2u);
    EXPECT_EQ(channel.read_count(), 0u);

    channel.swap_buffers();

    EXPECT_EQ(channel.read_count(), 2u);
    EXPECT_EQ(channel.write_count(), 0u);
    EXPECT_EQ(channel.read_buffer()[0].amount, 10);
    EXPECT_EQ(channel.read_buffer()[1].amount, 20);
}

TEST(EventChannel, SwapClearsWriteBuffer) {
    EventChannel<DamageEvent> channel;
    channel.send(DamageEvent{5});
    channel.swap_buffers();

    // Second swap: write buffer should be empty, read buffer should be empty
    channel.swap_buffers();
    EXPECT_EQ(channel.read_count(), 0u);
    EXPECT_EQ(channel.write_count(), 0u);
}

// ---- EventWriter tests ----

TEST(EventWriter, SendsToChannel) {
    EventChannel<DamageEvent> channel;
    EventWriter<DamageEvent> writer(&channel);
    writer.send(DamageEvent{99});
    EXPECT_EQ(channel.write_count(), 1u);
    channel.swap_buffers();
    EXPECT_EQ(channel.read_buffer()[0].amount, 99);
}

// ---- EventReader tests ----

TEST(EventReader, IteratesReadBuffer) {
    EventChannel<DamageEvent> channel;
    channel.send(DamageEvent{1});
    channel.send(DamageEvent{2});
    channel.send(DamageEvent{3});
    channel.swap_buffers();

    EventReader<DamageEvent> reader(&channel);
    EXPECT_EQ(reader.count(), 3u);
    int sum = 0;
    for (const auto& ev : reader) {
        sum += ev.amount;
    }
    EXPECT_EQ(sum, 6);
}

TEST(EventReader, EmptyWhenNoEvents) {
    EventChannel<DamageEvent> channel;
    EventReader<DamageEvent> reader(&channel);
    EXPECT_TRUE(reader.is_empty());
    EXPECT_EQ(reader.count(), 0u);
}

// ---- EventStorage tests ----

TEST(EventStorage, RegisterAndGetChannel) {
    EventStorage storage;
    storage.register_event<DamageEvent>();
    EXPECT_TRUE(storage.has_channel<DamageEvent>());
    auto& channel = storage.get_channel<DamageEvent>();
    channel.send(DamageEvent{7});
    EXPECT_EQ(channel.write_count(), 1u);
}

TEST(EventStorage, WriterAndReader) {
    EventStorage storage;
    storage.register_event<DamageEvent>();

    auto writer = storage.writer<DamageEvent>();
    writer.send(DamageEvent{15});
    writer.send(DamageEvent{30});

    storage.swap_all_buffers();

    auto reader = storage.reader<DamageEvent>();
    EXPECT_EQ(reader.count(), 2u);
    EXPECT_FALSE(reader.is_empty());

    int sum = 0;
    for (const auto& ev : reader) {
        sum += ev.amount;
    }
    EXPECT_EQ(sum, 45);
}

TEST(EventStorage, SwapAllBuffers) {
    EventStorage storage;
    storage.register_event<DamageEvent>();
    storage.register_event<SpawnEvent>();

    storage.writer<DamageEvent>().send(DamageEvent{5});
    storage.writer<SpawnEvent>().send(SpawnEvent{3});

    storage.swap_all_buffers();

    EXPECT_EQ(storage.reader<DamageEvent>().count(), 1u);
    EXPECT_EQ(storage.reader<SpawnEvent>().count(), 1u);
}

TEST(EventStorage, UnregisteredChannelAsserts) {
    EventStorage storage;
    EXPECT_DEATH_IF_SUPPORTED(storage.get_channel<DamageEvent>(), "event channel not registered");
    // writer() and reader() call get_channel() internally, so they would also assert.
}

TEST(EventStorage, StringEvents) {
    EventStorage storage;
    storage.register_event<std::string>();

    auto writer = storage.writer<std::string>();
    writer.send(std::string("hello"));
    writer.send(std::string("world"));

    storage.swap_all_buffers();

    auto reader = storage.reader<std::string>();
    EXPECT_EQ(reader.count(), 2u);

    std::string combined;
    for (const auto& s : reader) {
        combined += s;
    }
    EXPECT_EQ(combined, "helloworld");
}
