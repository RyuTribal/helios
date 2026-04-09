// helios-core/src/helios/assets/importers/shader_importer.h
//
// Importer for SPIR-V shader files (.spv). Reads the file as raw binary.
#pragma once

#include <any>
#include <filesystem>

namespace helios {

// Forward declaration
class AssetServer;

class ShaderImporter {
public:
    // Load a SPIR-V shader file. Returns ShaderAsset in std::any.
    static std::any import(const std::filesystem::path& path, AssetServer& server);
};

} // namespace helios
