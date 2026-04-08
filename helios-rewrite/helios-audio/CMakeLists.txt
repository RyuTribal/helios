cmake_minimum_required(VERSION 3.22)

# --- Attempt to fetch SoLoud ---
option(HELIOS_AUDIO_USE_SOLOUD "Use SoLoud audio backend (requires FetchContent)" ON)

set(HELIOS_AUDIO_HAS_SOLOUD OFF)

if(HELIOS_AUDIO_USE_SOLOUD)
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

        # Check that the files actually exist
        if(EXISTS "${SOLOUD_SRC_DIR}/core/soloud.cpp" AND EXISTS "${SOLOUD_SRC_DIR}/backend/miniaudio/soloud_miniaudio.cpp")
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

            set(HELIOS_AUDIO_HAS_SOLOUD ON)
            message(STATUS "helios-audio: SoLoud backend available (miniaudio)")
        else()
            message(WARNING "helios-audio: SoLoud source files not found, falling back to stub backend")
        endif()
    endif()
endif()

if(NOT HELIOS_AUDIO_HAS_SOLOUD)
    message(STATUS "helios-audio: Using stub audio backend only")
endif()

# --- helios-audio library ---
set(HELIOS_AUDIO_SOURCES
    src/stub/stub_audio_device.cpp
)

if(HELIOS_AUDIO_HAS_SOLOUD)
    list(APPEND HELIOS_AUDIO_SOURCES
        src/soloud/soloud_device.cpp
    )
endif()

add_library(helios-audio STATIC ${HELIOS_AUDIO_SOURCES})

target_include_directories(helios-audio
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/src
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/soloud
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/stub
)

target_link_libraries(helios-audio
    PUBLIC  helios-core
)

if(HELIOS_AUDIO_HAS_SOLOUD)
    target_link_libraries(helios-audio PRIVATE SoLoud)
    target_compile_definitions(helios-audio PUBLIC HELIOS_HAS_SOLOUD=1)

    # Suppress warnings on SoLoud headers in our TUs
    if(NOT MSVC)
        set_source_files_properties(
            src/soloud/soloud_device.cpp
            PROPERTIES COMPILE_OPTIONS
            "-Wno-unused-parameter;-Wno-missing-field-initializers;-Wno-old-style-cast"
        )
    endif()
else()
    target_compile_definitions(helios-audio PUBLIC HELIOS_HAS_SOLOUD=0)
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
        tests/test_stub_audio_device.cpp
        tests/test_audio_plugin.cpp
    )

    if(HELIOS_AUDIO_HAS_SOLOUD)
        list(APPEND HELIOS_AUDIO_TEST_SOURCES
            tests/test_soloud_device.cpp
        )
    endif()

    add_executable(helios-audio-tests ${HELIOS_AUDIO_TEST_SOURCES})

    target_link_libraries(helios-audio-tests
        PRIVATE helios-audio
        PRIVATE GTest::gtest_main
    )

    target_include_directories(helios-audio-tests
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    if(HELIOS_AUDIO_HAS_SOLOUD)
        target_link_libraries(helios-audio-tests PRIVATE SoLoud)
    endif()

    include(GoogleTest)
    gtest_discover_tests(helios-audio-tests)
endif()
