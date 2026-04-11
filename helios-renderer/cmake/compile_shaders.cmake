# helios-renderer/cmake/compile_shaders.cmake
#
# CMake function to compile GLSL shaders to SPIR-V via glslc.
#
# Usage:
#   compile_shaders(
#     SOURCE_DIR <path>        - directory containing .vert/.frag/.comp/.geom files
#     OUTPUT_DIR <path>        - directory for compiled .spv files
#     BACKEND    <vulkan|...>  - currently only vulkan (GLSL -> SPIR-V via glslc)
#     TARGET     <name>        - optional: add as dependency of this target
#   )

function(compile_shaders)
    cmake_parse_arguments(SHADER "" "SOURCE_DIR;OUTPUT_DIR;BACKEND;TARGET" "" ${ARGN})

    if(NOT SHADER_SOURCE_DIR)
        message(FATAL_ERROR "compile_shaders: SOURCE_DIR is required")
    endif()
    if(NOT SHADER_OUTPUT_DIR)
        message(FATAL_ERROR "compile_shaders: OUTPUT_DIR is required")
    endif()
    if(NOT SHADER_BACKEND)
        set(SHADER_BACKEND "vulkan")
    endif()

    # Find glslc (ships with Vulkan SDK)
    find_program(GLSLC_EXECUTABLE glslc)
    if(NOT GLSLC_EXECUTABLE)
        message(WARNING "glslc not found -- shader compilation disabled. "
                        "Install the Vulkan SDK or add glslc to PATH.")
        return()
    endif()

    # Collect all shader source files
    file(GLOB SHADER_SOURCES
        "${SHADER_SOURCE_DIR}/*.vert"
        "${SHADER_SOURCE_DIR}/*.frag"
        "${SHADER_SOURCE_DIR}/*.comp"
        "${SHADER_SOURCE_DIR}/*.geom"
    )

    if(NOT SHADER_SOURCES)
        message(STATUS "compile_shaders: no shader sources found in ${SHADER_SOURCE_DIR}")
        return()
    endif()

    # Ensure output directory exists
    file(MAKE_DIRECTORY "${SHADER_OUTPUT_DIR}")

    set(SPIRV_OUTPUTS "")

    foreach(SHADER_SRC ${SHADER_SOURCES})
        get_filename_component(SHADER_NAME ${SHADER_SRC} NAME)
        set(SPIRV_OUTPUT "${SHADER_OUTPUT_DIR}/${SHADER_NAME}.spv")

        # glslc auto-detects stage from .vert/.frag/.comp extensions,
        # but .geom needs an explicit stage flag.
        set(STAGE_FLAG "")
        if(SHADER_SRC MATCHES "\\.geom$")
            set(STAGE_FLAG "-fshader-stage=geometry")
        endif()

        add_custom_command(
            OUTPUT ${SPIRV_OUTPUT}
            COMMAND ${GLSLC_EXECUTABLE}
                    ${STAGE_FLAG}
                    -o ${SPIRV_OUTPUT}
                    ${SHADER_SRC}
            DEPENDS ${SHADER_SRC}
            COMMENT "Compiling shader: ${SHADER_NAME} -> ${SHADER_NAME}.spv"
            VERBATIM
        )

        list(APPEND SPIRV_OUTPUTS ${SPIRV_OUTPUT})
    endforeach()

    # Create a custom target for all shaders
    add_custom_target(helios_compile_shaders ALL DEPENDS ${SPIRV_OUTPUTS})

    # If a parent target is specified, add dependency
    if(SHADER_TARGET)
        add_dependencies(${SHADER_TARGET} helios_compile_shaders)
    endif()

    message(STATUS "compile_shaders: ${SHADER_BACKEND} backend, "
                   "${SHADER_SOURCE_DIR} -> ${SHADER_OUTPUT_DIR} "
                   "(${CMAKE_CURRENT_LIST_DIR})")
endfunction()
