add_executable(sandbox main.cpp)

target_link_libraries(sandbox
    PRIVATE
        helios-core
        helios-renderer
        glfw
)
target_compile_features(sandbox PRIVATE cxx_std_20)
