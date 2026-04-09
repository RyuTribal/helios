cmake_minimum_required(VERSION 3.22)

add_library(helios-script STATIC
    src/coreclr_runtime.cpp
    src/script_glue.cpp
    src/script_execution_system.cpp
    src/hot_reload_system.cpp
    src/scripting_plugin.cpp
    src/file_watcher.cpp
)

target_include_directories(helios-script
    PUBLIC  include
    PRIVATE src
)

target_link_libraries(helios-script
    PUBLIC  helios-core
    PRIVATE ${CMAKE_DL_LIBS}   # dlopen/dlsym on Linux
)

target_compile_features(helios-script PUBLIC cxx_std_20)

# Vendored .NET SDK headers (hostfxr.h, coreclr_delegates.h, nethost.h)
target_include_directories(helios-script
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/vendor/dotnet
)

# Path to the system .NET SDK for runtime hostfxr resolution.
# CoreCLRRuntime uses this to locate libhostfxr.so at runtime.
target_compile_definitions(helios-script PRIVATE
    HELIOS_DOTNET_ROOT="/usr/share/dotnet"
)

# Compiler warnings
if(MSVC)
    target_compile_options(helios-script PRIVATE /W4)
else()
    target_compile_options(helios-script PRIVATE -Wall -Wextra -Wpedantic)
endif()
