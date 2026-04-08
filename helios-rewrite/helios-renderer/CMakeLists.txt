cmake_minimum_required(VERSION 3.20)

option(HELIOS_BACKEND_VULKAN "Use Vulkan backend" ON)

# --- Vulkan SDK ---
find_package(Vulkan REQUIRED)

# --- VMA (header-only) ---
FetchContent_Declare(
    VulkanMemoryAllocator
    GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
    GIT_TAG        v3.1.0
)
FetchContent_MakeAvailable(VulkanMemoryAllocator)

# --- vk-bootstrap ---
FetchContent_Declare(
    vk-bootstrap
    GIT_REPOSITORY https://github.com/charles-lunarg/vk-bootstrap.git
    GIT_TAG        v1.3.296
)
FetchContent_MakeAvailable(vk-bootstrap)

# --- helios-renderer library ---
add_library(helios-renderer STATIC
    src/helios/vulkan/vulkan_context.cpp
    src/helios/vulkan/vulkan_device.cpp
    src/helios/vulkan/vulkan_shader.cpp
    src/helios/vulkan/vulkan_buffer.cpp
    src/helios/vulkan/vulkan_texture.cpp
    src/helios/vulkan/vulkan_vma.cpp
)

target_include_directories(helios-renderer
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(helios-renderer
    PUBLIC
        helios-core
    PRIVATE
        Vulkan::Vulkan
        GPUOpen::VulkanMemoryAllocator
        vk-bootstrap::vk-bootstrap
        glfw
)

target_compile_definitions(helios-renderer
    PUBLIC
        $<$<BOOL:${HELIOS_BACKEND_VULKAN}>:HELIOS_BACKEND_VULKAN>
)

target_compile_features(helios-renderer PUBLIC cxx_std_20)

# Debug: enable validation layer defines
target_compile_definitions(helios-renderer PRIVATE
    $<$<CONFIG:Debug>:HELIOS_VULKAN_VALIDATION=1>
    $<$<CONFIG:RelWithDebInfo>:HELIOS_VULKAN_VALIDATION=1>
)

# Compiler warnings
if(MSVC)
    target_compile_options(helios-renderer PRIVATE /W4)
else()
    target_compile_options(helios-renderer PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Suppress warnings in vendored VMA header (compiled via vulkan_vma.cpp)
if(NOT MSVC)
    set_source_files_properties(
        src/helios/vulkan/vulkan_vma.cpp
        PROPERTIES COMPILE_OPTIONS "-Wno-missing-field-initializers;-Wno-unused-parameter;-Wno-unused-variable;-Wno-unused-function"
    )
endif()
