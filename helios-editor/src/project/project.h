#pragma once

#include <helios/ecs/world.h>
#include <helios/ecs/thread_pool.h>

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace helios::editor {

// ---- Asset Registry (.hvereg) ----

enum class AssetType : uint8_t {
    Scene,
    MeshSource,
    Texture,
    CubeMap,
    Audio,
    Script,
    Material,
};

struct AssetRegistryEntry {
    uint64_t handle = 0;
    std::string file_path;
    AssetType type = AssetType::MeshSource;
};

struct AssetRegistry {
    std::vector<AssetRegistryEntry> entries;
    std::unordered_map<uint64_t, size_t> handle_to_index;

    void add(uint64_t handle, const std::string& path, AssetType type);
    void remove(uint64_t handle);
    const AssetRegistryEntry* find(uint64_t handle) const;
    const AssetRegistryEntry* find_by_path(const std::string& path) const;

    void save(const std::filesystem::path& path) const;
    void load(const std::filesystem::path& path);
};

// ---- Project (.hveproject) ----

struct RendererSettings {
    std::string anti_aliasing = "None";
    std::string post_processing = "None";
    int multiplier = 2;
    std::string present_mode = "Fifo";  // "Immediate", "Fifo", "Mailbox"
};

/// Async script build state. Inserted as a World resource.
struct ScriptBuildState {
    enum class Status { Idle, Building, Success, Failed };
    std::atomic<Status> status{Status::Idle};
    std::string output;

    bool is_building() const { return status.load() == Status::Building; }
};

struct Project {
    std::string name;
    std::filesystem::path project_dir;      // directory containing .hveproject
    std::string asset_path = "Assets";      // relative to project_dir
    std::string asset_registry_file = "AssetRegistry.hvereg";
    uint64_t starting_scene = 0;
    std::string script_assembly;            // relative path to compiled .dll
    RendererSettings renderer;

    AssetRegistry registry;

    // Resolved paths
    std::filesystem::path asset_dir() const { return project_dir / asset_path; }
    std::filesystem::path registry_path() const { return project_dir / asset_registry_file; }
    std::filesystem::path scene_dir() const { return asset_dir() / "Scenes"; }

    void save() const;
    void load(const std::filesystem::path& hveproject_path);

    static Project create_new(const std::filesystem::path& dir, const std::string& name,
                              ScriptBuildState* build_state = nullptr,
                              helios::ThreadPool* pool = nullptr);

    /// Apply this project's state to the world. Sets up:
    /// - AssetServer root to project's asset directory
    /// - ForwardPlusConfig skybox path
    /// - RenderSettings from project renderer settings
    /// - Loads starting scene if configured
    void apply_to_world(helios::World& world) const;

    /// Reset world state to clean no-project state.
    static void clear_from_world(helios::World& world);
};

} // namespace helios::editor
