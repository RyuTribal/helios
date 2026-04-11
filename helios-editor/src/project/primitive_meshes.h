#pragma once
#include <filesystem>

namespace helios::editor {

// Generate minimal .glb files for primitive meshes in the given directory.
void generate_primitive_meshes(const std::filesystem::path& meshes_dir);

} // namespace helios::editor
