// helios-core/src/helios/assets/importers/shader_importer.cpp
#include "helios/assets/importers/shader_importer.h"
#include "helios/assets/shader_asset.h"

#include <any>
#include <fstream>
#include <stdexcept>

namespace helios {

std::any ShaderImporter::import(const std::filesystem::path& path, AssetServer& /*server*/) {
    if (!std::filesystem::exists(path)) {
        throw std::runtime_error("Shader file not found: " + path.string());
    }

    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open shader: " + path.string());
    }

    auto size = file.tellg();
    ShaderAsset asset;
    asset.spirv.resize(static_cast<size_t>(size));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(asset.spirv.data()), size);

    return std::any(std::move(asset));
}

} // namespace helios
