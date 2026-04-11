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
