cmake_minimum_required(VERSION 3.22)

# --- Fetch Jolt Physics (hard requirement) ---
include(FetchContent)

# Jolt build options -- must be set before FetchContent
set(JOLT_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(JOLT_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(JOLT_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
set(JOLT_DEBUG_RENDERER OFF CACHE BOOL "" FORCE)
set(JOLT_DOUBLE_PRECISION OFF CACHE BOOL "" FORCE)
set(JOLT_OBJECT_LAYER_BITS 16 CACHE STRING "" FORCE)
set(JOLT_ENABLE_ASSERTS OFF CACHE BOOL "" FORCE)
set(INTERPROCEDURAL_OPTIMIZATION OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    JoltPhysics
    GIT_REPOSITORY https://github.com/jrouwe/JoltPhysics.git
    GIT_TAG        v5.2.0
    SOURCE_SUBDIR  Build
)

FetchContent_MakeAvailable(JoltPhysics)

if(NOT TARGET Jolt)
    message(FATAL_ERROR "helios-physics: Jolt Physics is required but could not be fetched")
endif()

message(STATUS "helios-physics: Jolt Physics backend available")

# --- helios-physics library ---
set(HELIOS_PHYSICS_SOURCES
    src/jolt/jolt_physics_world.cpp
    src/interface/physics_factory.cpp
)

add_library(helios-physics STATIC ${HELIOS_PHYSICS_SOURCES})

target_include_directories(helios-physics
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/src
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/jolt
)

target_link_libraries(helios-physics
    PUBLIC  helios-core
    PRIVATE Jolt
)

# Suppress warnings on Jolt headers in our TUs
if(NOT MSVC)
    set_source_files_properties(
        src/jolt/jolt_physics_world.cpp
        src/interface/physics_factory.cpp
        PROPERTIES COMPILE_OPTIONS
        "-Wno-unused-parameter;-Wno-missing-field-initializers;-Wno-old-style-cast"
    )
endif()

target_compile_features(helios-physics PUBLIC cxx_std_20)

# Compiler warnings
if(MSVC)
    target_compile_options(helios-physics PRIVATE /W4)
else()
    target_compile_options(helios-physics PRIVATE -Wall -Wextra -Wpedantic)
endif()

# --- Tests ---
if(HELIOS_BUILD_TESTS)
    set(HELIOS_PHYSICS_TEST_SOURCES
        tests/test_physics_plugin.cpp
        tests/test_jolt_physics_world.cpp
    )

    add_executable(helios-physics-tests ${HELIOS_PHYSICS_TEST_SOURCES})

    target_link_libraries(helios-physics-tests
        PRIVATE helios-physics
        PRIVATE GTest::gtest_main
        PRIVATE Jolt
    )

    target_include_directories(helios-physics-tests
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    include(GoogleTest)
    gtest_discover_tests(helios-physics-tests)
endif()
