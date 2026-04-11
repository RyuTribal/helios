#include "project.h"
#include "primitive_meshes.h"
#include "../editor_state.h"

#include <helios/assets/asset_utils.h>
#include <helios/assets/asset_binary.h>
#include <helios/components/components.h>
#include <helios/render_settings.h>
#include <helios/forward_plus/forward_plus_config.h>
#include <helios/serialization/scene_serializer.h>

#include <helios/core/log_macros.h>

#include <yaml-cpp/yaml.h>
#include <cstdio>
#include <fstream>

HELIOS_DECLARE_LOG_CHANNEL(Assets);

namespace helios::editor {

// ---- AssetRegistry ----

void AssetRegistry::add(uint64_t handle, const std::string& path, AssetType type) {
    auto it = handle_to_index.find(handle);
    if (it != handle_to_index.end()) {
        entries[it->second] = {handle, path, type};
        return;
    }
    handle_to_index[handle] = entries.size();
    entries.push_back({handle, path, type});
}

void AssetRegistry::remove(uint64_t handle) {
    auto it = handle_to_index.find(handle);
    if (it == handle_to_index.end()) return;
    size_t idx = it->second;
    if (idx < entries.size() - 1) {
        entries[idx] = entries.back();
        handle_to_index[entries[idx].handle] = idx;
    }
    entries.pop_back();
    handle_to_index.erase(it);
}

const AssetRegistryEntry* AssetRegistry::find(uint64_t handle) const {
    auto it = handle_to_index.find(handle);
    if (it == handle_to_index.end()) return nullptr;
    return &entries[it->second];
}

const AssetRegistryEntry* AssetRegistry::find_by_path(const std::string& path) const {
    for (auto& e : entries) {
        if (e.file_path == path) return &e;
    }
    return nullptr;
}

static const char* asset_type_to_string(AssetType t) {
    switch (t) {
        case AssetType::Scene:      return "Scene";
        case AssetType::MeshSource: return "MeshSource";
        case AssetType::Texture:    return "Texture";
        case AssetType::CubeMap:    return "CubeMap";
        case AssetType::Audio:      return "Audio";
        case AssetType::Script:     return "Script";
        case AssetType::Material:   return "Material";
    }
    return "Unknown";
}

static AssetType string_to_asset_type(const std::string& s) {
    if (s == "Scene")      return AssetType::Scene;
    if (s == "MeshSource") return AssetType::MeshSource;
    if (s == "Texture")    return AssetType::Texture;
    if (s == "CubeMap")    return AssetType::CubeMap;
    if (s == "Audio")      return AssetType::Audio;
    if (s == "Script")     return AssetType::Script;
    if (s == "Material")   return AssetType::Material;
    return AssetType::MeshSource;
}

void AssetRegistry::save(const std::filesystem::path& path) const {
    YAML::Emitter out;
    out << YAML::BeginMap;
    out << YAML::Key << "AssetRegistry" << YAML::Value;
    out << YAML::BeginSeq;
    for (auto& e : entries) {
        out << YAML::BeginMap;
        out << YAML::Key << "Handle" << YAML::Value << e.handle;
        out << YAML::Key << "FilePath" << YAML::Value << e.file_path;
        out << YAML::Key << "Type" << YAML::Value << asset_type_to_string(e.type);
        out << YAML::EndMap;
    }
    out << YAML::EndSeq;
    out << YAML::EndMap;

    std::ofstream file(path);
    file << out.c_str();
}

void AssetRegistry::load(const std::filesystem::path& path) {
    entries.clear();
    handle_to_index.clear();

    if (!std::filesystem::exists(path)) return;

    YAML::Node root = YAML::LoadFile(path.string());
    auto reg = root["AssetRegistry"];
    if (!reg || !reg.IsSequence()) return;

    for (auto node : reg) {
        uint64_t handle = node["Handle"].as<uint64_t>(0);
        std::string fp = node["FilePath"].as<std::string>("");
        AssetType type = string_to_asset_type(node["Type"].as<std::string>("MeshSource"));
        add(handle, fp, type);
    }
}

// ---- Project ----

void Project::save() const {
    YAML::Emitter out;
    out << YAML::BeginMap;
    out << YAML::Key << "Project" << YAML::Value << name;
    out << YAML::Key << "AssetPath" << YAML::Value << asset_path;
    out << YAML::Key << "AssetRegistry" << YAML::Value << asset_registry_file;
    out << YAML::Key << "StartingScene" << YAML::Value << starting_scene;
    out << YAML::Key << "ScriptAssembly" << YAML::Value << script_assembly;
    out << YAML::Key << "Renderer" << YAML::Value;
    out << YAML::BeginMap;
    out << YAML::Key << "PresentMode" << YAML::Value << renderer.present_mode;
    out << YAML::Key << "AntiAliasing" << YAML::Value;
    out << YAML::BeginMap;
    out << YAML::Key << "Type" << YAML::Value << renderer.anti_aliasing;
    out << YAML::Key << "PostProcessing" << YAML::Value << renderer.post_processing;
    out << YAML::Key << "Multiplier" << YAML::Value << renderer.multiplier;
    out << YAML::EndMap;
    out << YAML::EndMap;
    out << YAML::EndMap;

    auto proj_file = project_dir / (name + ".hveproject");
    std::ofstream file(proj_file);
    file << out.c_str();

    registry.save(registry_path());
}

void Project::load(const std::filesystem::path& hveproject_path) {
    project_dir = hveproject_path.parent_path();

    YAML::Node root = YAML::LoadFile(hveproject_path.string());
    name = root["Project"].as<std::string>("Untitled");
    asset_path = root["AssetPath"].as<std::string>("Assets");
    asset_registry_file = root["AssetRegistry"].as<std::string>("AssetRegistry.hvereg");
    starting_scene = root["StartingScene"].as<uint64_t>(0);
    script_assembly = root["ScriptAssembly"].as<std::string>("");

    auto renderer_node = root["Renderer"];
    if (renderer_node) {
        renderer.present_mode = renderer_node["PresentMode"].as<std::string>("Fifo");
        auto aa = renderer_node["AntiAliasing"];
        if (aa) {
            renderer.anti_aliasing = aa["Type"].as<std::string>("None");
            renderer.post_processing = aa["PostProcessing"].as<std::string>("None");
            renderer.multiplier = aa["Multiplier"].as<int>(2);
        }
    }

    registry.load(registry_path());
}

static void generate_csproj(const std::filesystem::path& scripts_dir,
                            const std::string& proj_name,
                            const std::filesystem::path& scriptcore_dll) {
    auto csproj_path = scripts_dir / (proj_name + ".csproj");
    std::ofstream csproj(csproj_path);
    csproj << R"(<Project Sdk="Microsoft.NET.Sdk">
  <PropertyGroup>
    <TargetFramework>net10.0</TargetFramework>
    <ImplicitUsings>disable</ImplicitUsings>
    <Nullable>disable</Nullable>
    <AllowUnsafeBlocks>true</AllowUnsafeBlocks>
  </PropertyGroup>
  <ItemGroup>
    <Reference Include="ScriptCore">
      <HintPath>)" << scriptcore_dll.string() << R"(</HintPath>
    </Reference>
  </ItemGroup>
</Project>
)";
}

static void build_script_dll_async(ScriptBuildState& state,
                                    const std::filesystem::path& scripts_dir,
                                    helios::ThreadPool& pool) {
    state.status.store(ScriptBuildState::Status::Building);
    state.output.clear();

    pool.submit([&state, dir = scripts_dir.string()]() {
        std::string cmd = "cd '" + dir + "' && dotnet build -c Release -v q 2>&1";
        FILE* pipe = popen(cmd.c_str(), "r");
        if (!pipe) {
            state.output = "Failed to run dotnet build";
            state.status.store(ScriptBuildState::Status::Failed);
            return;
        }
        char buf[256];
        while (fgets(buf, sizeof(buf), pipe))
            state.output += buf;
        int exit_code = pclose(pipe);
        state.status.store(exit_code == 0
            ? ScriptBuildState::Status::Success
            : ScriptBuildState::Status::Failed);
    });
}

Project Project::create_new(const std::filesystem::path& dir, const std::string& proj_name,
                             ScriptBuildState* build_state, helios::ThreadPool* pool) {
    Project proj;
    proj.name = proj_name;
    proj.project_dir = dir / proj_name;

    // Create directory structure
    std::filesystem::create_directories(proj.project_dir);
    std::filesystem::create_directories(proj.asset_dir());
    std::filesystem::create_directories(proj.scene_dir());
    std::filesystem::create_directories(proj.asset_dir() / "Meshes");
    std::filesystem::create_directories(proj.asset_dir() / "Textures");
    std::filesystem::create_directories(proj.asset_dir() / "Sounds");

    // --- C# scripting project ---
    auto scripts_dir = proj.project_dir / "Scripts";
    std::filesystem::create_directories(scripts_dir);

    auto scriptcore_dll = std::filesystem::current_path() /
        "ScriptCore" / "bin" / "Release" / "net10.0" / "ScriptCore.dll";
    generate_csproj(scripts_dir, proj_name, scriptcore_dll);
    proj.script_assembly = "Scripts/bin/Release/net10.0/" + proj_name + ".dll";
    if (build_state && pool) {
        build_script_dll_async(*build_state, scripts_dir, *pool);
    }

    // --- Primitive meshes (Cube, Sphere, Plane) ---
    // generate_primitive_meshes now writes optimized .hvemesh directly
    // (PBRVertex binary), no intermediate .glb files.
    generate_primitive_meshes(proj.asset_dir() / "Meshes");

    // Register Helios binary files in the asset registry
    proj.registry.add(100, "Meshes/Cube.hvemesh",   AssetType::MeshSource);
    proj.registry.add(101, "Meshes/Sphere.hvemesh",  AssetType::MeshSource);
    proj.registry.add(102, "Meshes/Plane.hvemesh",   AssetType::MeshSource);

    // --- Skybox HDR ---
    // Copy from sandbox assets if available. The .hdr is loaded directly
    // by the CubeMapAsset importer (no intermediate .hvecube needed here;
    // import_asset() creates optimized .hvecube when importing via the editor).
    auto sandbox_hdr = std::filesystem::current_path() /
        "sandbox" / "assets" / "Textures" / "default_skybox.hdr";
    auto dest_hdr = proj.asset_dir() / "Textures" / "default_skybox.hdr";
    if (std::filesystem::exists(sandbox_hdr)) {
        std::filesystem::copy_file(sandbox_hdr, dest_hdr,
            std::filesystem::copy_options::skip_existing);

        proj.registry.add(200, "Textures/default_skybox.hdr", AssetType::CubeMap);
    }

    // --- Default scene ---
    auto default_scene = proj.scene_dir() / "Main_Scene.hvescn";
    {
        std::ofstream scene(default_scene);
        scene << "Scene:\n  Handle: 1\n  Name: Main_Scene\nEntities:\n  []\n";
    }

    proj.save();
    return proj;
}

void Project::apply_to_world(helios::World& world) const {
    // 1. Update AssetServer root
    if (world.has_resource<std::shared_ptr<helios::AssetServer>>()) {
        auto& server = world.resource<std::shared_ptr<helios::AssetServer>>();
        server->set_root(asset_dir());
    }

    // 2. Auto-import skybox .hdr → .hvecube if needed, then set path
    {
        auto skybox_hvecube = asset_dir() / "Textures" / "default_skybox.hvecube";
        auto skybox_hdr     = asset_dir() / "Textures" / "default_skybox.hdr";

        // Auto-import: if .hdr exists but .hvecube doesn't, import it
        if (!std::filesystem::exists(skybox_hvecube) && std::filesystem::exists(skybox_hdr)) {
            if (world.has_resource<std::shared_ptr<helios::AssetServer>>()) {
                auto& server = world.resource<std::shared_ptr<helios::AssetServer>>();
                server->import_asset(skybox_hdr, helios::AssetBinaryType::CubeMap,
                                     "Textures/default_skybox.hvecube");
            }
        }

        if (auto* fpc = world.try_resource<helios::ForwardPlusConfig>()) {
            if (std::filesystem::exists(skybox_hvecube)) {
                fpc->skybox_hdr_path = "Textures/default_skybox.hvecube";
            } else if (std::filesystem::exists(skybox_hdr)) {
                fpc->skybox_hdr_path = "Textures/default_skybox.hdr";
            } else {
                fpc->skybox_hdr_path.clear();
            }
        }
    }

    // 3. Sync RenderSettings from project renderer settings
    if (auto* rs = world.try_resource<helios::RenderSettings>()) {
        if (renderer.present_mode == "Immediate")
            rs->set_present_mode(helios::rhi::PresentMode::Immediate);
        else if (renderer.present_mode == "Mailbox")
            rs->set_present_mode(helios::rhi::PresentMode::Mailbox);
        else
            rs->set_present_mode(helios::rhi::PresentMode::Fifo);
    }

    // 4. Load or create the starting scene
    {
        auto scene_path = scene_dir() / "Main_Scene.hvescn";
        std::string relative_path = "Scenes/Main_Scene.hvescn";
        std::string scene_name = name.empty() ? "Main_Scene" : name;

        helios::Entity scene_root{};
        if (auto* serializer = world.try_resource<helios::SceneSerializer>()) {
            if (std::filesystem::exists(scene_path)) {
                scene_root = serializer->load_scene(world, scene_path);
            }
        }

        // If no SceneRoot exists (empty/old scene), create one
        if (!world.is_alive(scene_root)) {
            scene_root = world.spawn(
                helios::Tag{.name = scene_name},
                helios::Transform{},
                helios::SceneRoot{
                    .scene_name = scene_name,
                    .scene_path = relative_path,
                }
            );
        } else {
            auto* sr = world.try_get<helios::SceneRoot>(scene_root);
            if (sr && sr->scene_path.empty()) {
                sr->scene_path = relative_path;
            }
        }

        if (auto* edit = world.try_resource<helios::editor::SceneEditState>()) {
            edit->active_scene = scene_root;
        }
    }

    // 5. Resolve asset paths on loaded MeshRenderers
    helios::resolve_mesh_paths(world);
}

void Project::clear_from_world(helios::World& world) {
    // Reset skybox
    if (auto* fpc = world.try_resource<helios::ForwardPlusConfig>()) {
        fpc->skybox_hdr_path.clear();
    }

    // Reset render settings to defaults
    if (auto* rs = world.try_resource<helios::RenderSettings>()) {
        *rs = helios::RenderSettings{};
    }

    // Reset asset server root
    if (world.has_resource<std::shared_ptr<helios::AssetServer>>()) {
        auto& server = world.resource<std::shared_ptr<helios::AssetServer>>();
        server->set_root(".");
    }
}

} // namespace helios::editor
