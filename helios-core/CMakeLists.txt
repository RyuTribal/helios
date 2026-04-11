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
        src/helios/ecs/transform_propagation.cpp
        src/helios/ecs/app.cpp
        src/helios/app/state.cpp
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
        src/helios/assets/asset_server.cpp
        src/helios/assets/handle.cpp
        src/helios/assets/load_batch.cpp
        src/helios/assets/asset_plugin.cpp
        src/helios/assets/asset_binary.cpp
        src/helios/assets/importers/texture_importer.cpp
        src/helios/assets/importers/mesh_importer.cpp
        src/helios/assets/importers/audio_importer.cpp
        src/helios/assets/importers/shader_importer.cpp
        src/helios/serialization/yaml_serializer.cpp
        src/helios/serialization/scene_serializer.cpp
        src/helios/scene/scene_manager.cpp
        src/helios/scene/scene_plugin.cpp
)

# Headers and sources co-located under src/ — #include "helios/ecs/entity.h" works
target_include_directories(helios-core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor
)

target_link_libraries(helios-core
    PUBLIC
        glm::glm
        spdlog::spdlog
        yaml-cpp::yaml-cpp
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

# Suppress vendor-header warnings in importer TUs that include stb/cgltf
if(NOT MSVC)
    set_source_files_properties(
        src/helios/assets/importers/texture_importer.cpp
        src/helios/assets/importers/mesh_importer.cpp
        PROPERTIES COMPILE_OPTIONS
        "-Wno-missing-field-initializers;-Wno-unused-parameter;-Wno-unused-variable;-Wno-unused-function;-Wno-sign-compare;-Wno-unused-but-set-variable;-Wno-missing-braces"
    )
endif()
