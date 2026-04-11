// helios-rewrite/tests/core/test_assert.cpp
//
// Tests for HELIOS_ASSERT, HELIOS_VERIFY, and HELIOS_UNREACHABLE macros.
//
#include <helios/core/assert.h>

#include <gtest/gtest.h>

// ---- HELIOS_ASSERT tests ----

TEST(AssertTest, PassingAssertIsNoOp) {
    // Should not abort -- just a no-op.
    HELIOS_ASSERT(true);
    HELIOS_ASSERT(1 == 1);
    HELIOS_ASSERT(42 > 0, "positive number");
}

TEST(AssertDeathTest, FailingAssertAborts) {
    EXPECT_DEATH(HELIOS_ASSERT(false), "ASSERT FAILED");
}

TEST(AssertDeathTest, FailingAssertWithMessageAborts) {
    EXPECT_DEATH(HELIOS_ASSERT(false, "custom message"), "custom message");
}

// ---- HELIOS_VERIFY tests ----

TEST(VerifyTest, PassingVerifyIsNoOp) {
    HELIOS_VERIFY(true);
    HELIOS_VERIFY(1 == 1, "should pass");
}

TEST(VerifyDeathTest, FailingVerifyAborts) {
    EXPECT_DEATH(HELIOS_VERIFY(false, "verify msg"), "verify msg");
}

// ---- HELIOS_UNREACHABLE tests ----

TEST(UnreachableDeathTest, UnreachableAborts) {
    EXPECT_DEATH(HELIOS_UNREACHABLE("bad path"), "UNREACHABLE");
}

TEST(UnreachableDeathTest, UnreachableShowsMessage) {
    EXPECT_DEATH(HELIOS_UNREACHABLE("bad path"), "bad path");
}
