#include <gtest/gtest.h>
#include <typeindex>
#include <vector>
#include "helios/ecs/dag_builder.h"
#include "helios/ecs/access_descriptor.h"
#include "helios/ecs/system_descriptor.h"

using namespace helios;

struct CompA {};
struct CompB {};
struct CompC {};
struct MyEvent {};

// Helper: build a minimal SystemDescriptor with given accesses
static SystemDescriptor make_system(SystemId id, std::vector<AccessDescriptor> accesses,
                                    std::vector<SystemId> after = {},
                                    std::vector<SystemId> before = {}) {
    SystemDescriptor desc;
    desc.id       = id;
    desc.name     = "sys_" + std::to_string(id.value);
    desc.run      = [](World&, uint32_t) {};
    desc.accesses = std::move(accesses);
    desc.after    = std::move(after);
    desc.before   = std::move(before);
    return desc;
}

TEST(DAGBuilder, EmptySystems) {
    auto plan = build_execution_plan({});
    EXPECT_TRUE(plan.stages.empty());
}

TEST(DAGBuilder, SingleSystem) {
    std::vector<SystemDescriptor> systems = {
        make_system({1}, {
            { std::type_index(typeid(CompA)), AccessMode::Write }
        }),
    };
    auto plan = build_execution_plan(systems);
    ASSERT_EQ(plan.stages.size(), 1u);
    ASSERT_EQ(plan.stages[0].system_indices.size(), 1u);
    EXPECT_EQ(plan.stages[0].system_indices[0], 0u);
}

TEST(DAGBuilder, IndependentSystemsSameStage) {
    // System 0 writes CompA, System 1 writes CompB -- no conflict -> same stage
    std::vector<SystemDescriptor> systems = {
        make_system({1}, { { std::type_index(typeid(CompA)), AccessMode::Write } }),
        make_system({2}, { { std::type_index(typeid(CompB)), AccessMode::Write } }),
    };
    auto plan = build_execution_plan(systems);
    ASSERT_EQ(plan.stages.size(), 1u);
    EXPECT_EQ(plan.stages[0].system_indices.size(), 2u);
}

TEST(DAGBuilder, ConflictingSystemsSeparateStages) {
    // Both write CompA -> conflict -> separate stages
    std::vector<SystemDescriptor> systems = {
        make_system({1}, { { std::type_index(typeid(CompA)), AccessMode::Write } }),
        make_system({2}, { { std::type_index(typeid(CompA)), AccessMode::Write } }),
    };
    auto plan = build_execution_plan(systems);
    ASSERT_EQ(plan.stages.size(), 2u);
    EXPECT_EQ(plan.stages[0].system_indices.size(), 1u);
    EXPECT_EQ(plan.stages[1].system_indices.size(), 1u);
}

TEST(DAGBuilder, ReadReadNoConflict) {
    // Both read CompA -> no conflict -> same stage
    std::vector<SystemDescriptor> systems = {
        make_system({1}, { { std::type_index(typeid(CompA)), AccessMode::Read } }),
        make_system({2}, { { std::type_index(typeid(CompA)), AccessMode::Read } }),
    };
    auto plan = build_execution_plan(systems);
    ASSERT_EQ(plan.stages.size(), 1u);
}

TEST(DAGBuilder, ReadWriteConflict) {
    // System 0 reads CompA, System 1 writes CompA -> conflict
    std::vector<SystemDescriptor> systems = {
        make_system({1}, { { std::type_index(typeid(CompA)), AccessMode::Read } }),
        make_system({2}, { { std::type_index(typeid(CompA)), AccessMode::Write } }),
    };
    auto plan = build_execution_plan(systems);
    ASSERT_EQ(plan.stages.size(), 2u);
}

TEST(DAGBuilder, ExplicitAfterOrdering) {
    // System 1 writes CompA, System 2 writes CompB (no conflict),
    // but System 2 must run after System 1.
    std::vector<SystemDescriptor> systems = {
        make_system({1}, { { std::type_index(typeid(CompA)), AccessMode::Write } }),
        make_system({2}, { { std::type_index(typeid(CompB)), AccessMode::Write } },
                    /*after=*/{ SystemId{1} }),
    };
    auto plan = build_execution_plan(systems);
    // They would be in the same stage without the constraint, but the
    // explicit after forces separate stages.
    ASSERT_EQ(plan.stages.size(), 2u);
    EXPECT_EQ(plan.stages[0].system_indices[0], 0u);
    EXPECT_EQ(plan.stages[1].system_indices[0], 1u);
}

TEST(DAGBuilder, ExplicitBeforeOrdering) {
    // System 2 must run before System 1 (both write different types).
    std::vector<SystemDescriptor> systems = {
        make_system({1}, { { std::type_index(typeid(CompA)), AccessMode::Write } }),
        make_system({2}, { { std::type_index(typeid(CompB)), AccessMode::Write } },
                    /*after=*/{}, /*before=*/{ SystemId{1} }),
    };
    auto plan = build_execution_plan(systems);
    ASSERT_EQ(plan.stages.size(), 2u);
    // System 2 (index 1) should be in stage 0, System 1 (index 0) in stage 1
    EXPECT_EQ(plan.stages[0].system_indices[0], 1u); // system 2 first
    EXPECT_EQ(plan.stages[1].system_indices[0], 0u); // system 1 second
}

TEST(DAGBuilder, ThreeSystemsDiamond) {
    // System 0: writes A
    // System 1: writes B (after 0)
    // System 2: writes C (after 0)
    // System 3: reads A, B, C (after 1, after 2)
    //
    // Expected stages:
    //   Stage 0: [0]
    //   Stage 1: [1, 2]  (parallel -- no conflict)
    //   Stage 2: [3]
    std::vector<SystemDescriptor> systems = {
        make_system({1}, { { std::type_index(typeid(CompA)), AccessMode::Write } }),
        make_system({2}, { { std::type_index(typeid(CompB)), AccessMode::Write } },
                    /*after=*/{ SystemId{1} }),
        make_system({3}, { { std::type_index(typeid(CompC)), AccessMode::Write } },
                    /*after=*/{ SystemId{1} }),
        make_system({4}, {
            { std::type_index(typeid(CompA)), AccessMode::Read },
            { std::type_index(typeid(CompB)), AccessMode::Read },
            { std::type_index(typeid(CompC)), AccessMode::Read },
        }, /*after=*/{ SystemId{2}, SystemId{3} }),
    };
    auto plan = build_execution_plan(systems);
    ASSERT_EQ(plan.stages.size(), 3u);
    EXPECT_EQ(plan.stages[0].system_indices.size(), 1u); // [0]
    EXPECT_EQ(plan.stages[1].system_indices.size(), 2u); // [1, 2]
    EXPECT_EQ(plan.stages[2].system_indices.size(), 1u); // [3]
}

TEST(DAGBuilder, RegistrationOrderTiebreak) {
    // Two systems that conflict: both write A.
    // No explicit ordering. The DAG builder should order them by registration
    // index (0 before 1).
    std::vector<SystemDescriptor> systems = {
        make_system({1}, { { std::type_index(typeid(CompA)), AccessMode::Write } }),
        make_system({2}, { { std::type_index(typeid(CompA)), AccessMode::Write } }),
    };
    auto plan = build_execution_plan(systems);
    ASSERT_EQ(plan.stages.size(), 2u);
    EXPECT_EQ(plan.stages[0].system_indices[0], 0u); // registered first -> runs first
    EXPECT_EQ(plan.stages[1].system_indices[0], 1u);
}

// ---- Event conflict tests (regression for typeid(T) fix) ----

// Two EventWriter<MyEvent> systems both declare Write on typeid(MyEvent).
// They must be sequenced (write-write conflict).
TEST(DAGBuilder, EventWriterWriterConflict) {
    std::vector<SystemDescriptor> systems = {
        make_system({1}, { { std::type_index(typeid(MyEvent)), AccessMode::Write } }),
        make_system({2}, { { std::type_index(typeid(MyEvent)), AccessMode::Write } }),
    };
    auto plan = build_execution_plan(systems);
    ASSERT_EQ(plan.stages.size(), 2u) << "Two EventWriter<T> systems must not run in parallel";
}

// EventReader<MyEvent> (Read on typeid(MyEvent)) vs EventWriter<MyEvent>
// (Write on typeid(MyEvent)) must also conflict (read-write).
TEST(DAGBuilder, EventReaderWriterConflict) {
    std::vector<SystemDescriptor> systems = {
        make_system({1}, { { std::type_index(typeid(MyEvent)), AccessMode::Read } }),
        make_system({2}, { { std::type_index(typeid(MyEvent)), AccessMode::Write } }),
    };
    auto plan = build_execution_plan(systems);
    ASSERT_EQ(plan.stages.size(), 2u) << "EventReader<T> and EventWriter<T> must not run in parallel";
}

// Two EventReader<MyEvent> systems both declare Read on typeid(MyEvent).
// Readers never conflict -- they must be placed in the same stage.
TEST(DAGBuilder, EventReaderReaderNoConflict) {
    std::vector<SystemDescriptor> systems = {
        make_system({1}, { { std::type_index(typeid(MyEvent)), AccessMode::Read } }),
        make_system({2}, { { std::type_index(typeid(MyEvent)), AccessMode::Read } }),
    };
    auto plan = build_execution_plan(systems);
    ASSERT_EQ(plan.stages.size(), 1u) << "Two EventReader<T> systems must be allowed to run in parallel";
}
