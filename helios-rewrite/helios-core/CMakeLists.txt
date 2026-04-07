add_library(helios-core STATIC)

# Sources will be added as tasks progress - start with a placeholder
target_sources(helios-core
    PRIVATE
        src/ecs/entity.cpp
        src/ecs/entity_allocator.cpp
        src/ecs/archetype.cpp
        src/ecs/archetype_storage.cpp
        src/ecs/resource_storage.cpp
        src/ecs/event_storage.cpp
        src/ecs/world.cpp
        src/ecs/commands.cpp
)

target_include_directories(helios-core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(helios-core
    PUBLIC
        glm::glm
)

target_compile_features(helios-core PUBLIC cxx_std_20)

# Compiler warnings
if(MSVC)
    target_compile_options(helios-core PRIVATE /W4)
else()
    target_compile_options(helios-core PRIVATE -Wall -Wextra -Wpedantic)
endif()
