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
        src/helios/core/uuid.cpp
)

# Headers and sources co-located under src/ — #include "helios/ecs/entity.h" works
target_include_directories(helios-core
    PUBLIC
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
