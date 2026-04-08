add_library(helios-core STATIC)

target_sources(helios-core
    PRIVATE
        src/helios/ecs/entity.cpp
        src/helios/ecs/entity_allocator.cpp
        src/helios/ecs/archetype.cpp
        src/helios/ecs/archetype_storage.cpp
        src/helios/ecs/resource_storage.cpp
        src/helios/ecs/event_storage.cpp
        src/helios/ecs/world.cpp
        src/helios/ecs/commands.cpp
        src/helios/ecs/thread_pool.cpp
        src/helios/ecs/dag_builder.cpp
        src/helios/ecs/scheduler.cpp
        src/helios/ecs/app.cpp
        src/helios/core/uuid.cpp
        src/helios/core/log_entry.cpp
        src/helios/core/log_channel_registry.cpp
        src/helios/core/log_system.cpp
        src/helios/core/engine_log_channels.cpp
        src/helios/core/assert.cpp
        src/helios/core/crash_handler.cpp
        src/helios/window/window.cpp
        src/helios/window/windows.cpp
        src/helios/window/window_plugin.cpp
        src/helios/input/raw_input.cpp
        src/helios/input/input_map.cpp
        src/helios/input/input_systems.cpp
        src/helios/input/input_plugin.cpp
)

# Headers and sources co-located under src/ — #include "helios/ecs/entity.h" works
target_include_directories(helios-core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(helios-core
    PUBLIC
        glm::glm
        spdlog::spdlog
    PRIVATE
        glfw
)

target_compile_features(helios-core PUBLIC cxx_std_20)

# Compile-time log level configuration.
# Debug:   HELIOS_LOG_ACTIVE_LEVEL=0 (Trace -- everything)
# Release: HELIOS_LOG_ACTIVE_LEVEL=2 (Info  -- strip Trace+Debug)
# Dist:    HELIOS_LOG_ACTIVE_LEVEL=3 (Warn  -- strip Trace+Debug+Info)
target_compile_definitions(helios-core
    PUBLIC
        $<$<CONFIG:Debug>:HELIOS_LOG_ACTIVE_LEVEL=0>
        $<$<CONFIG:Release>:HELIOS_LOG_ACTIVE_LEVEL=2>
        $<$<CONFIG:RelWithDebInfo>:HELIOS_LOG_ACTIVE_LEVEL=1>
        $<$<CONFIG:MinSizeRel>:HELIOS_LOG_ACTIVE_LEVEL=3>
)

# Compiler warnings
if(MSVC)
    target_compile_options(helios-core PRIVATE /W4)
else()
    target_compile_options(helios-core PRIVATE -Wall -Wextra -Wpedantic)
endif()
