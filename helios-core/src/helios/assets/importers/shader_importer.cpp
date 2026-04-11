#include "helios/assets/importers/shader_importer.h"
#include "helios/assets/shader_asset.h"
#include "helios/core/engine_log_channels.h"

#include <any>
#include <fstream>

namespace helios {

std::any ShaderImporter::import(const std::filesystem::path& path, AssetServer& /*server*/) {
    if (!std::filesystem::exists(path)) {
        HELIOS_LOG(Assets, Error, "Shader file not found: {}", path.string());
        return std::any{};
    }

    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        HELIOS_LOG(Assets, Error, "Failed to open shader: {}", path.string());
        return std::any{};
    }

    auto size = file.tellg();
    ShaderAsset asset;
    asset.spirv.resize(static_cast<size_t>(size));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(asset.spirv.data()), size);

    return std::any(std::move(asset));
}

} // namespace helios
