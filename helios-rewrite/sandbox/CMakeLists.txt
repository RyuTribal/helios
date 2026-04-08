add_executable(sandbox
    main.cpp
    asset_loader.cpp
)

target_include_directories(sandbox PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor
)

target_link_libraries(sandbox
    PRIVATE
        helios-core
        helios-renderer
        glfw
)
target_compile_features(sandbox PRIVATE cxx_std_20)

# Asset paths (hardcoded for this one-off demo)
target_compile_definitions(sandbox PRIVATE
    HELIOS_SHADER_DIR="${CMAKE_BINARY_DIR}/shaders"
    HELIOS_DEMO_ASSET_DIR="/home/ryutribal/programming/helios_test/helios_test/Assets"
)

# Suppress vendor header warnings in asset_loader.cpp
if(NOT MSVC)
    set_source_files_properties(
        asset_loader.cpp
        PROPERTIES COMPILE_OPTIONS
        "-Wno-missing-field-initializers;-Wno-unused-parameter;-Wno-unused-variable;-Wno-unused-function;-Wno-sign-compare;-Wno-unused-but-set-variable;-Wno-missing-braces"
    )
endif()
