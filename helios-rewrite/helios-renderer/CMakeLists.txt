cmake_minimum_required(VERSION 3.20)

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
    # RHI factory (runtime backend selection)
    src/helios/rhi/rhi_factory.cpp
    src/helios/render_plugin.cpp
    # Vulkan backend
    src/helios/vulkan/vulkan_context.cpp
    src/helios/vulkan/vulkan_device.cpp
    src/helios/vulkan/vulkan_shader.cpp
    src/helios/vulkan/vulkan_buffer.cpp
    src/helios/vulkan/vulkan_texture.cpp
    src/helios/vulkan/vulkan_render_pass.cpp
    src/helios/vulkan/vulkan_framebuffer.cpp
    src/helios/vulkan/vulkan_descriptor.cpp
    src/helios/vulkan/vulkan_pipeline.cpp
    src/helios/vulkan/vulkan_command_buffer.cpp
    src/helios/vulkan/vulkan_swapchain.cpp
    src/helios/vulkan/vulkan_vma.cpp
    src/helios/pipeline_cache.cpp
    # Forward+ pipeline
    src/helios/forward_plus/forward_plus_plugin.cpp
    src/helios/forward_plus/extract_render_data.cpp
    src/helios/forward_plus/default_textures.cpp
    src/helios/forward_plus/pipeline_init.cpp
    src/helios/forward_plus/forward_plus_draw.cpp
    # IBL one-shot compute dispatches
    src/helios/forward_plus/ibl/equirect_to_cube.cpp
    src/helios/forward_plus/ibl/ibl_generation.cpp
    # Forward+ pass nodes
    src/helios/forward_plus/passes/depth_prepass.cpp
    src/helios/forward_plus/passes/shadow_pass.cpp
    src/helios/forward_plus/passes/light_culling_pass.cpp
    src/helios/forward_plus/passes/forward_pass.cpp
    src/helios/forward_plus/passes/skybox_pass.cpp
    src/helios/forward_plus/passes/tonemap_pass.cpp
    # Render graph
    src/helios/graph/render_graph.cpp
    src/helios/graph/render_graph_builder.cpp
    src/helios/graph/render_context.cpp
    src/helios/graph/resource_pool.cpp
    # Render thread
    src/helios/graph/render_thread.cpp
)

target_include_directories(helios-renderer
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(helios-renderer
    PUBLIC
        helios-core
        Vulkan::Vulkan
        GPUOpen::VulkanMemoryAllocator
        vk-bootstrap::vk-bootstrap
    PRIVATE
        glfw
)

target_compile_features(helios-renderer PUBLIC cxx_std_20)

# Debug: enable validation layer defines
target_compile_definitions(helios-renderer PRIVATE
    $<$<CONFIG:Debug>:HELIOS_VULKAN_VALIDATION=1>
    $<$<CONFIG:RelWithDebInfo>:HELIOS_VULKAN_VALIDATION=1>
)

# Shader directory path for runtime loading
target_compile_definitions(helios-renderer PRIVATE
    HELIOS_SHADER_DIR="${CMAKE_BINARY_DIR}/shaders"
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

# --- Shader compilation (GLSL -> SPIR-V via glslc) ---
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/compile_shaders.cmake)

compile_shaders(
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shaders
    OUTPUT_DIR ${CMAKE_BINARY_DIR}/shaders
    BACKEND    vulkan
    TARGET     helios-renderer
)

# Tests
if(HELIOS_BUILD_TESTS)
    add_subdirectory(tests)
endif()
