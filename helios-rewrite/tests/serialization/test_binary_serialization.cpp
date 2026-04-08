#include <gtest/gtest.h>

#include "helios/ecs/asset_handle.h"
#include "helios/serialization/binary_helpers.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <sstream>
#include <string>

namespace helios::test {

// ----- Scalar round-trips -----

TEST(BinaryHelpersTest, FloatRoundTrip) {
    std::ostringstream out(std::ios::binary);
    write_binary(out, 3.14f);

    std::istringstream in(out.str(), std::ios::binary);
    float v = 0.0f;
    read_binary(in, v);
    EXPECT_EQ(v, 3.14f);
}

TEST(BinaryHelpersTest, DoubleRoundTrip) {
    std::ostringstream out(std::ios::binary);
    write_binary(out, 2.718281828);

    std::istringstream in(out.str(), std::ios::binary);
    double v = 0.0;
    read_binary(in, v);
    EXPECT_EQ(v, 2.718281828);
}

TEST(BinaryHelpersTest, IntRoundTrip) {
    std::ostringstream out(std::ios::binary);
    write_binary(out, -42);

    std::istringstream in(out.str(), std::ios::binary);
    int v = 0;
    read_binary(in, v);
    EXPECT_EQ(v, -42);
}

TEST(BinaryHelpersTest, Uint32RoundTrip) {
    std::ostringstream out(std::ios::binary);
    write_binary(out, uint32_t{99});

    std::istringstream in(out.str(), std::ios::binary);
    uint32_t v = 0;
    read_binary(in, v);
    EXPECT_EQ(v, 99u);
}

TEST(BinaryHelpersTest, Uint64RoundTrip) {
    std::ostringstream out(std::ios::binary);
    write_binary(out, uint64_t{123456789012345});

    std::istringstream in(out.str(), std::ios::binary);
    uint64_t v = 0;
    read_binary(in, v);
    EXPECT_EQ(v, 123456789012345u);
}

TEST(BinaryHelpersTest, BoolRoundTrip) {
    std::ostringstream out(std::ios::binary);
    write_binary(out, true);
    write_binary(out, false);

    std::istringstream in(out.str(), std::ios::binary);
    bool a = false, b = true;
    read_binary(in, a);
    read_binary(in, b);
    EXPECT_TRUE(a);
    EXPECT_FALSE(b);
}

// ----- String round-trips -----

TEST(BinaryHelpersTest, StringRoundTrip) {
    std::ostringstream out(std::ios::binary);
    write_binary(out, std::string("hello world"));

    std::istringstream in(out.str(), std::ios::binary);
    std::string v;
    read_binary(in, v);
    EXPECT_EQ(v, "hello world");
}

TEST(BinaryHelpersTest, EmptyStringRoundTrip) {
    std::ostringstream out(std::ios::binary);
    write_binary(out, std::string(""));

    std::istringstream in(out.str(), std::ios::binary);
    std::string v = "notempty";
    read_binary(in, v);
    EXPECT_EQ(v, "");
}

// ----- AssetHandle round-trip -----

TEST(BinaryHelpersTest, AssetHandleRoundTrip) {
    std::ostringstream out(std::ios::binary);
    write_binary(out, AssetHandle{42, 7});

    std::istringstream in(out.str(), std::ios::binary);
    AssetHandle v;
    read_binary(in, v);
    EXPECT_EQ(v.index, 42u);
    EXPECT_EQ(v.generation, 7u);
}

TEST(BinaryHelpersTest, NullAssetHandleRoundTrip) {
    std::ostringstream out(std::ios::binary);
    write_binary(out, AssetHandle{});

    std::istringstream in(out.str(), std::ios::binary);
    AssetHandle v{99, 1};
    read_binary(in, v);
    EXPECT_EQ(v.index, 0u);
    EXPECT_EQ(v.generation, 0u);
}

// ----- glm type round-trips -----

TEST(BinaryHelpersTest, Vec2RoundTrip) {
    std::ostringstream out(std::ios::binary);
    write_binary(out, glm::vec2{1.5f, -2.3f});

    std::istringstream in(out.str(), std::ios::binary);
    glm::vec2 v{};
    read_binary(in, v);
    EXPECT_EQ(v.x, 1.5f);
    EXPECT_EQ(v.y, -2.3f);
}

TEST(BinaryHelpersTest, Vec3RoundTrip) {
    std::ostringstream out(std::ios::binary);
    write_binary(out, glm::vec3{1.0f, 2.0f, 3.0f});

    std::istringstream in(out.str(), std::ios::binary);
    glm::vec3 v{};
    read_binary(in, v);
    EXPECT_EQ(v.x, 1.0f);
    EXPECT_EQ(v.y, 2.0f);
    EXPECT_EQ(v.z, 3.0f);
}

TEST(BinaryHelpersTest, Vec4RoundTrip) {
    std::ostringstream out(std::ios::binary);
    write_binary(out, glm::vec4{1.0f, 2.0f, 3.0f, 4.0f});

    std::istringstream in(out.str(), std::ios::binary);
    glm::vec4 v{};
    read_binary(in, v);
    EXPECT_EQ(v.x, 1.0f);
    EXPECT_EQ(v.y, 2.0f);
    EXPECT_EQ(v.z, 3.0f);
    EXPECT_EQ(v.w, 4.0f);
}

TEST(BinaryHelpersTest, QuatRoundTrip) {
    glm::quat original{0.707f, 0.0f, 0.707f, 0.0f};

    std::ostringstream out(std::ios::binary);
    write_binary(out, original);

    std::istringstream in(out.str(), std::ios::binary);
    glm::quat v{};
    read_binary(in, v);
    EXPECT_EQ(v.w, original.w);
    EXPECT_EQ(v.x, original.x);
    EXPECT_EQ(v.y, original.y);
    EXPECT_EQ(v.z, original.z);
}

TEST(BinaryHelpersTest, Mat4RoundTrip) {
    glm::mat4 original(1.0f);
    original[0][0] = 2.0f;
    original[1][2] = 3.5f;
    original[3][3] = 7.0f;

    std::ostringstream out(std::ios::binary);
    write_binary(out, original);

    std::istringstream in(out.str(), std::ios::binary);
    glm::mat4 v{};
    read_binary(in, v);

    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            EXPECT_EQ(v[col][row], original[col][row])
                << "Mismatch at [" << col << "][" << row << "]";
        }
    }
}

// ----- Enum round-trip -----

enum class TestEnum : uint8_t { A = 0, B = 1, C = 2 };

TEST(BinaryHelpersTest, EnumRoundTrip) {
    std::ostringstream out(std::ios::binary);
    write_binary(out, TestEnum::B);

    std::istringstream in(out.str(), std::ios::binary);
    TestEnum v = TestEnum::A;
    read_binary(in, v);
    EXPECT_EQ(v, TestEnum::B);
}

// ----- Multiple fields in sequence -----

TEST(BinaryHelpersTest, MultipleFieldsSequence) {
    std::ostringstream out(std::ios::binary);
    write_binary(out, glm::vec3{1.0f, 2.0f, 3.0f});
    write_binary(out, glm::quat{1.0f, 0.0f, 0.0f, 0.0f});
    write_binary(out, glm::vec3{1.0f, 1.0f, 1.0f});

    std::istringstream in(out.str(), std::ios::binary);
    glm::vec3 pos{};
    glm::quat rot{};
    glm::vec3 scale{};
    read_binary(in, pos);
    read_binary(in, rot);
    read_binary(in, scale);

    EXPECT_EQ(pos, glm::vec3(1.0f, 2.0f, 3.0f));
    EXPECT_EQ(rot.w, 1.0f);
    EXPECT_EQ(scale, glm::vec3(1.0f, 1.0f, 1.0f));

    // Total size: vec3(12) + quat(16) + vec3(12) = 40 bytes
    EXPECT_EQ(out.str().size(), 40u);
}

} // namespace helios::test
