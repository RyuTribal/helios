# helios-editor/CMakeLists.txt
#
# Editor executable — the ONLY target that links ImGui, ImGuizmo, nfd.
# No engine library (helios-core, helios-renderer, etc.) may include
# any ImGui header or reference any ImGui type.

include(FetchContent)

# ---------------------------------------------------------------------------
# Dear ImGui (docking branch) -- no CMakeLists, build from source
# ---------------------------------------------------------------------------
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        docking
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(imgui)

add_library(imgui_lib STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    # Backends
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
)

target_include_directories(imgui_lib PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

find_package(Vulkan REQUIRED)
target_link_libraries(imgui_lib PUBLIC
    Vulkan::Vulkan
    glfw
)

# Suppress warnings in vendored imgui code
target_compile_options(imgui_lib PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-w>
)

# ---------------------------------------------------------------------------
# ImGuizmo -- no CMakeLists, build from source
# ---------------------------------------------------------------------------
FetchContent_Declare(
    imguizmo
    GIT_REPOSITORY https://github.com/CedricGuillemet/ImGuizmo.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(imguizmo)

# ImGuizmo wrapper: needs imgui.h + IMGUI_DEFINE_MATH_OPERATORS before ImGuizmo.h
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/imguizmo_wrapper.cpp
    "#define IMGUI_DEFINE_MATH_OPERATORS\n#include <imgui.h>\n#include \"ImGuizmo.cpp\"\n")

add_library(imguizmo_lib STATIC
    ${CMAKE_CURRENT_BINARY_DIR}/imguizmo_wrapper.cpp
)

target_include_directories(imguizmo_lib PUBLIC
    ${imguizmo_SOURCE_DIR}
)

target_link_libraries(imguizmo_lib PUBLIC imgui_lib)

target_compile_options(imguizmo_lib PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-w>
)

# ---------------------------------------------------------------------------
# Native File Dialog Extended
# ---------------------------------------------------------------------------
FetchContent_Declare(
    nfd
    GIT_REPOSITORY https://github.com/btzy/nativefiledialog-extended.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE
)
set(NFD_PORTAL OFF CACHE BOOL "" FORCE)  # GTK backend (more reliable on Linux)
FetchContent_MakeAvailable(nfd)

# ---------------------------------------------------------------------------
# helios-editor executable
# ---------------------------------------------------------------------------
add_executable(helios-editor
    src/main.cpp
    src/editor_plugin.cpp
    src/editor_commands.cpp
    src/editor_camera.cpp
    src/editor_resources.cpp
    src/imgui/imgui_render_plugin.cpp
    src/panels/scene_hierarchy.cpp
    src/panels/inspector.cpp
    src/panels/content_browser.cpp
    src/panels/viewport.cpp
    src/panels/project_settings.cpp
    src/panels/scene_settings.cpp
    src/panels/console.cpp
    src/panels/import_modal.cpp
    src/panels/export_modal.cpp
    src/gizmo/gizmo_system.cpp
    src/project/project.cpp
    src/project/primitive_meshes.cpp
)

target_include_directories(helios-editor PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Link ALL engine libraries
target_link_libraries(helios-editor PRIVATE
    helios-core
    helios-renderer
    helios-physics
    helios-audio
    helios-script
)

# Editor-only dependencies — NEVER appear in engine targets
target_link_libraries(helios-editor PRIVATE
    imgui_lib
    imguizmo_lib
    nfd
)

target_compile_definitions(helios-editor PRIVATE
    HELIOS_EDITOR=1
    IMGUI_USE_WCHAR32=1
)

# ImGui and ImGuizmo also need WCHAR32
target_compile_definitions(imgui_lib PUBLIC IMGUI_USE_WCHAR32=1)

target_compile_features(helios-editor PUBLIC cxx_std_20)

# Copy editor resources (fonts, icons) next to the binary
add_custom_command(TARGET helios-editor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/resources
        $<TARGET_FILE_DIR:helios-editor>/resources
)

set_target_properties(helios-editor PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
