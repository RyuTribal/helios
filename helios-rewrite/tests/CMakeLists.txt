add_executable(helios-tests
    ecs/test_placeholder.cpp
)

target_link_libraries(helios-tests
    PRIVATE
        helios-core
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(helios-tests)
