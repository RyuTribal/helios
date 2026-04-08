add_executable(helios-renderer-tests
    test_rhi_types.cpp
    test_vulkan_device.cpp
    test_vulkan_resources.cpp
    test_rhi_integration.cpp
)

target_link_libraries(helios-renderer-tests
    PRIVATE
        helios-renderer
        glfw
        GTest::gtest_main
)

target_include_directories(helios-renderer-tests PRIVATE
    ${CMAKE_SOURCE_DIR}/helios-renderer/src
)

include(GoogleTest)
gtest_discover_tests(helios-renderer-tests)
