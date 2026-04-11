#include <gtest/gtest.h>

#include "helios/assets/binary_writer.h"
#include "helios/assets/binary_reader.h"

#include <cstdint>
#include <string>
#include <vector>

namespace helios::test {

// ---- BinaryWriter Tests ----

TEST(BinaryWriter, InitiallyEmpty) {
    BinaryWriter w;
    EXPECT_EQ(w.size(), 0u);
}

TEST(BinaryWriter, WriteScalar) {
    BinaryWriter w;
    w.write<uint32_t>(42);
    EXPECT_EQ(w.size(), sizeof(uint32_t));
}

TEST(BinaryWriter, WriteString) {
    BinaryWriter w;
    w.write_string("hello");
    // 4 bytes length prefix + 5 bytes content
    EXPECT_EQ(w.size(), 4u + 5u);
}

TEST(BinaryWriter, WriteEmptyString) {
    BinaryWriter w;
    w.write_string("");
    // 4 bytes length prefix + 0 bytes content
    EXPECT_EQ(w.size(), 4u);
}

TEST(BinaryWriter, WriteBytes) {
    BinaryWriter w;
    std::vector<uint8_t> bytes = {0xDE, 0xAD, 0xBE, 0xEF};
    w.write_bytes(bytes);
    EXPECT_EQ(w.size(), 4u);
    EXPECT_EQ(w.data()[0], 0xDE);
    EXPECT_EQ(w.data()[3], 0xEF);
}

TEST(BinaryWriter, Take) {
    BinaryWriter w;
    w.write<uint32_t>(123);
    auto buf = w.take();
    EXPECT_EQ(buf.size(), sizeof(uint32_t));
    // After take, writer is in a moved-from state
}

TEST(BinaryWriter, Reserve) {
    BinaryWriter w;
    w.reserve(1024);
    EXPECT_EQ(w.size(), 0u); // reserve doesn't change size
    w.write<uint32_t>(1);
    EXPECT_EQ(w.size(), sizeof(uint32_t));
}

// ---- BinaryReader Tests ----

TEST(BinaryReader, ReadScalarRoundTrip) {
    BinaryWriter w;
    w.write<uint32_t>(42);
    w.write<float>(3.14f);
    w.write<int16_t>(-7);

    BinaryReader r(w.data(), w.size());
    EXPECT_EQ(r.read<uint32_t>(), 42u);
    EXPECT_EQ(r.read<float>(), 3.14f);
    EXPECT_EQ(r.read<int16_t>(), -7);
    EXPECT_TRUE(r.at_end());
}

TEST(BinaryReader, ReadStringRoundTrip) {
    BinaryWriter w;
    w.write_string("hello world");
    w.write_string("");
    w.write_string("test");

    BinaryReader r(w.data(), w.size());
    EXPECT_EQ(r.read_string(), "hello world");
    EXPECT_EQ(r.read_string(), "");
    EXPECT_EQ(r.read_string(), "test");
    EXPECT_TRUE(r.at_end());
}

TEST(BinaryReader, OffsetAndRemaining) {
    BinaryWriter w;
    w.write<uint64_t>(0);
    w.write<uint32_t>(0);

    BinaryReader r(w.data(), w.size());
    EXPECT_EQ(r.offset(), 0u);
    EXPECT_EQ(r.remaining(), 12u);

    r.read<uint64_t>();
    EXPECT_EQ(r.offset(), 8u);
    EXPECT_EQ(r.remaining(), 4u);

    r.read<uint32_t>();
    EXPECT_EQ(r.offset(), 12u);
    EXPECT_EQ(r.remaining(), 0u);
    EXPECT_TRUE(r.at_end());
}

TEST(BinaryReader, Skip) {
    BinaryWriter w;
    w.write<uint32_t>(111);
    w.write<uint32_t>(222);
    w.write<uint32_t>(333);

    BinaryReader r(w.data(), w.size());
    r.skip(sizeof(uint32_t)); // skip first
    EXPECT_EQ(r.read<uint32_t>(), 222u);
    r.skip(sizeof(uint32_t)); // skip third
    EXPECT_TRUE(r.at_end());
}

TEST(BinaryReader, ReadBytesRaw) {
    BinaryWriter w;
    std::vector<uint8_t> original = {0x01, 0x02, 0x03, 0x04, 0x05};
    w.write_bytes(original);

    BinaryReader r(w.data(), w.size());
    uint8_t buf[5] = {};
    r.read_bytes(buf, 5);
    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(buf[i], original[i]);
    }
    EXPECT_TRUE(r.at_end());
}

TEST(BinaryReader, OverrunReturnsDefault) {
    BinaryWriter w;
    w.write<uint8_t>(0);

    BinaryReader r(w.data(), w.size());
    // Overrun returns zero-initialized value and moves cursor to end
    auto val = r.read<uint32_t>();
    EXPECT_EQ(val, 0u);
    EXPECT_TRUE(r.at_end());
}

TEST(BinaryReader, SkipOverrunReturnsFalse) {
    BinaryWriter w;
    w.write<uint8_t>(0);

    BinaryReader r(w.data(), w.size());
    EXPECT_FALSE(r.skip(100));
    EXPECT_TRUE(r.at_end());
}

TEST(BinaryReader, StringOverrunReturnsEmpty) {
    // Write a string length prefix that claims more data than available
    BinaryWriter w;
    w.write<uint32_t>(9999); // length prefix says 9999 bytes
    w.write<uint8_t>(0);     // but only 1 byte of data

    BinaryReader r(w.data(), w.size());
    auto str = r.read_string();
    EXPECT_TRUE(str.empty());
    EXPECT_TRUE(r.at_end());
}

TEST(BinaryReader, EmptyBuffer) {
    BinaryReader r(nullptr, 0);
    EXPECT_TRUE(r.at_end());
    EXPECT_EQ(r.remaining(), 0u);
    EXPECT_EQ(r.offset(), 0u);
}

// ---- Combined read/write sequences ----

TEST(BinaryReadWrite, MixedTypesRoundTrip) {
    BinaryWriter w;
    w.write<uint64_t>(0xDEADBEEFCAFEBABEULL);
    w.write_string("metadata_key");
    w.write<float>(2.718f);
    w.write_string("metadata_value");
    w.write<uint8_t>(0xFF);

    BinaryReader r(w.data(), w.size());
    EXPECT_EQ(r.read<uint64_t>(), 0xDEADBEEFCAFEBABEULL);
    EXPECT_EQ(r.read_string(), "metadata_key");
    EXPECT_EQ(r.read<float>(), 2.718f);
    EXPECT_EQ(r.read_string(), "metadata_value");
    EXPECT_EQ(r.read<uint8_t>(), 0xFF);
    EXPECT_TRUE(r.at_end());
}

TEST(BinaryReadWrite, LargePayload) {
    BinaryWriter w;
    std::vector<uint8_t> payload(10000, 0xAB);
    w.write_bytes(payload);

    BinaryReader r(w.data(), w.size());
    std::vector<uint8_t> readback(10000);
    r.read_bytes(readback.data(), 10000);
    EXPECT_EQ(readback, payload);
    EXPECT_TRUE(r.at_end());
}

} // namespace helios::test
