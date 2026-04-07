add_executable(helios-tests
    ecs/test_entity.cpp
    ecs/test_entity_allocator.cpp
    ecs/test_archetype.cpp
    ecs/test_archetype_storage.cpp
    ecs/test_resources.cpp
    ecs/test_events.cpp
    ecs/test_query.cpp
)

target_link_libraries(helios-tests
    PRIVATE
        helios-core
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(helios-tests)
