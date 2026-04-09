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

# Asset paths (hardcoded for this one-off demo)
target_compile_definitions(sandbox PRIVATE
    HELIOS_DEMO_ASSET_DIR="/home/ryutribal/programming/helios_test/helios_test/Assets"
)

# Compiler warnings
if(MSVC)
    target_compile_options(sandbox PRIVATE /W4)
else()
    target_compile_options(sandbox PRIVATE -Wall -Wextra -Wpedantic)
endif()
