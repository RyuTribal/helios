#pragma once

#include <cstdint>
#include <filesystem>

namespace helios {

// Forward declarations
class App;

struct AssetPluginConfig {
    std::filesystem::path asset_root = "assets";
    uint32_t loader_threads = 2;
    bool hot_reload = false;
};

struct AssetPlugin {
    AssetPluginConfig config;

    explicit AssetPlugin(AssetPluginConfig cfg = {})
        : config(std::move(cfg)) {}

    void build(App& app);
};

} // namespace helios
