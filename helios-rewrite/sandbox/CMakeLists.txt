add_executable(sandbox
    main.cpp
)

target_link_libraries(sandbox
    PRIVATE
        helios-core
        helios-renderer
        helios-physics
        helios-audio
        glfw
)
target_compile_features(sandbox PRIVATE cxx_std_20)

# Shader path (compiled SPIR-V output)
target_compile_definitions(sandbox PRIVATE
    HELIOS_SHADER_DIR="${CMAKE_BINARY_DIR}/shaders"
)

# Compiler warnings
if(MSVC)
    target_compile_options(sandbox PRIVATE /W4)
else()
    target_compile_options(sandbox PRIVATE -Wall -Wextra -Wpedantic)
endif()
