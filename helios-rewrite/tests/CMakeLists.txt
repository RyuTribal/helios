add_executable(helios-tests
    ecs/test_entity.cpp
    ecs/test_entity_allocator.cpp
    ecs/test_archetype.cpp
    ecs/test_archetype_storage.cpp
    ecs/test_resources.cpp
    ecs/test_events.cpp
    ecs/test_query.cpp
    ecs/test_world.cpp
    ecs/test_commands.cpp
    ecs/test_components.cpp
    core/test_uuid.cpp
)

target_link_libraries(helios-tests
    PRIVATE
        helios-core
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(helios-tests)
