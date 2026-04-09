cmake_minimum_required(VERSION 3.22)

# --- Fetch SoLoud (hard requirement) ---
include(FetchContent)

FetchContent_Declare(
    soloud
    GIT_REPOSITORY https://github.com/jarikomppa/soloud.git
    GIT_TAG        RELEASE_20200207
)

FetchContent_GetProperties(soloud)
if(NOT soloud_POPULATED)
    FetchContent_Populate(soloud)

    # SoLoud does not provide a standard CMakeLists.txt at the repo root.
    # We build it manually from the audiosource and core sources with
    # the miniaudio backend.
    set(SOLOUD_SRC_DIR "${soloud_SOURCE_DIR}/src")

    # Collect core + miniaudio backend sources
    file(GLOB SOLOUD_CORE_SOURCES "${SOLOUD_SRC_DIR}/core/*.cpp")
    set(SOLOUD_BACKEND_SOURCES "${SOLOUD_SRC_DIR}/backend/miniaudio/soloud_miniaudio.cpp")
    file(GLOB SOLOUD_AUDIOSOURCE_SOURCES "${SOLOUD_SRC_DIR}/audiosource/**/*.cpp" "${SOLOUD_SRC_DIR}/audiosource/**/*.c")

    if(NOT EXISTS "${SOLOUD_SRC_DIR}/core/soloud.cpp" OR NOT EXISTS "${SOLOUD_SRC_DIR}/backend/miniaudio/soloud_miniaudio.cpp")
        message(FATAL_ERROR "helios-audio: SoLoud source files not found — SoLoud is a hard requirement")
    endif()

    add_library(SoLoud STATIC
        ${SOLOUD_CORE_SOURCES}
        ${SOLOUD_BACKEND_SOURCES}
        ${SOLOUD_AUDIOSOURCE_SOURCES}
    )

    target_include_directories(SoLoud PUBLIC "${soloud_SOURCE_DIR}/include")
    target_compile_definitions(SoLoud PUBLIC WITH_MINIAUDIO)
    target_compile_features(SoLoud PUBLIC cxx_std_17)

    # Suppress all warnings on vendored SoLoud code
    if(NOT MSVC)
        target_compile_options(SoLoud PRIVATE -w)
    else()
        target_compile_options(SoLoud PRIVATE /w)
    endif()

    message(STATUS "helios-audio: SoLoud backend available (miniaudio)")
endif()

# --- helios-audio library ---
set(HELIOS_AUDIO_SOURCES
    src/soloud/soloud_device.cpp
    src/interface/audio_factory.cpp
)

add_library(helios-audio STATIC ${HELIOS_AUDIO_SOURCES})

target_include_directories(helios-audio
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/src
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/soloud
)

target_link_libraries(helios-audio
    PUBLIC  helios-core
    PRIVATE SoLoud
)

# Suppress warnings on SoLoud headers in our TUs
if(NOT MSVC)
    set_source_files_properties(
        src/soloud/soloud_device.cpp
        PROPERTIES COMPILE_OPTIONS
        "-Wno-unused-parameter;-Wno-missing-field-initializers;-Wno-old-style-cast"
    )
endif()

target_compile_features(helios-audio PUBLIC cxx_std_20)

# Compiler warnings
if(MSVC)
    target_compile_options(helios-audio PRIVATE /W4)
else()
    target_compile_options(helios-audio PRIVATE -Wall -Wextra -Wpedantic)
endif()

# --- Tests ---
if(HELIOS_BUILD_TESTS)
    set(HELIOS_AUDIO_TEST_SOURCES
        tests/test_audio_plugin.cpp
        tests/test_soloud_device.cpp
    )

    add_executable(helios-audio-tests ${HELIOS_AUDIO_TEST_SOURCES})

    target_link_libraries(helios-audio-tests
        PRIVATE helios-audio
        PRIVATE GTest::gtest_main
        PRIVATE SoLoud
    )

    target_include_directories(helios-audio-tests
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    include(GoogleTest)
    gtest_discover_tests(helios-audio-tests)
endif()
