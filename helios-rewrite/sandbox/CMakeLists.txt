add_executable(sandbox main.cpp)

target_link_libraries(sandbox PRIVATE helios-core)
target_compile_features(sandbox PRIVATE cxx_std_20)
