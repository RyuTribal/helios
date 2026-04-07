add_executable(helios-tests
    ecs/test_entity.cpp
    ecs/test_entity_allocator.cpp
)

target_link_libraries(helios-tests
    PRIVATE
        helios-core
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(helios-tests)
