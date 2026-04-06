#include "pch.h"
#include "ProjectSerializer.h"
#include "Scene/SceneSerializer.h"
#include "Serialization/YAMLSerializer.h"
#include "Scene/Scene.h"
#include "Assets/AssetTypes.h"
#include "Assets/AssetManager.h"
#include "Project.h"
#include "Assets/DesignAssetManager.h"
#include "Core/IO.h"

namespace Engine {

	static std::string file_extension = ".hveproject";

	std::pair<bool, std::filesystem::path> ProjectSerializer::CreateNewProject(const std::filesystem::path& directory, const std::string& name)
	{
		if (!std::filesystem::exists(directory))
		{
			HVE_CORE_ERROR_TAG("Project Serializer", "Directory does not exist!");
			return { false, "" };
		}

		std::filesystem::path full_dir_path = directory / std::filesystem::path(name);
		if (std::filesystem::exists(full_dir_path))
		{
			HVE_CORE_ERROR_TAG("Project Serializer", "The folder {0} already exists!", std::filesystem::absolute(full_dir_path).string());
			return { false, "" };
		}


		std::filesystem::create_directory(full_dir_path);
		std::filesystem::path asset_path = full_dir_path / "Assets";
		std::filesystem::create_directory(asset_path);

		std::vector<std::string> sub_dirs = { "Meshes", "Scenes", "Scripts", "Meshes/Primitives", "Textures"};

		for (auto& dir : sub_dirs)
		{
			std::filesystem::path new_dir = asset_path / dir;
			std::filesystem::create_directory(new_dir);
		}


		ProjectSettings settings{};
		settings.ProjectName = name;
		settings.RootPath = full_dir_path;
		settings.AssetPath = std::filesystem::path("Assets");
		settings.ScriptAssemblyPath = std::filesystem::path("Binaries/" + name + ".dll");

		auto new_project = CreateRef<Project>(settings);
		Project::SetActive(new_project);
		auto asset_manager = new_project->GetDesignAssetManager();

		auto default_scene = Scene::CreateScene("Main_Scene");
		std::filesystem::path root_path = ROOT_PATH;
		root_path = root_path.parent_path();
		std::filesystem::path default_skybox_path = root_path / "Editor/Resources/Images/default_skybox.hdr";
		std::filesystem::path skybox_dest = asset_path / "Textures/default_skybox.hdr";
		std::filesystem::copy_file(default_skybox_path, skybox_dest, std::filesystem::copy_options::skip_existing);
		auto skybox_handle = Project::GetActiveDesignAssetManager()->ImportAsset(skybox_dest);
		SkyboxSettings skybox{};
		skybox.Texture = AssetManager::GetAsset<TextureCube>(skybox_handle);
		default_scene->SetSkybox(skybox);


		SceneSerializer::Serializer(asset_path / "Scenes", default_scene.get());
		settings.StartingScene = default_scene->Handle;

		std::filesystem::path scene_file_path = asset_path / std::filesystem::path("Scenes") / std::filesystem::path("Main_Scene.hvescn");
		asset_manager->RegisterAsset(default_scene->Handle, scene_file_path);
		CreateScriptProject();

		std::filesystem::path primitives_source = root_path / "Editor/Resources/Primitives";
		std::filesystem::path primitives_destination = asset_path / "Meshes/Primitives";
		for (const auto& entry : std::filesystem::recursive_directory_iterator(primitives_source))
		{
			const auto& primitive_source = entry.path();
			const auto& file = std::filesystem::relative(primitive_source, primitives_source);
			const auto& primitive_destination = primitives_destination / file;
			if (std::filesystem::is_regular_file(primitive_source))
			{
				try
				{
					std::filesystem::copy_file(primitive_source, primitive_destination, std::filesystem::copy_options::skip_existing);
				}
				catch (const std::filesystem::filesystem_error& e)
				{
					HVE_CORE_ERROR("Error copying file: {}", e.what());
				}

				const auto& registration_path = std::filesystem::relative(primitive_destination, asset_path);
				Project::GetActiveDesignAssetManager()->ImportAsset(primitive_destination);
			}
		}

		Serializer(settings);
		return { true, settings.RootPath / std::filesystem::path(settings.ProjectName + file_extension) };
	}

	void ProjectSerializer::Serializer(ProjectSettings& settings)
	{		
		auto renderer_settings = Renderer::Get()->GetSettings();
		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "Project" << YAML::Value << settings.ProjectName;
		out << YAML::Key << "AssetPath" << YAML::Value << settings.AssetPath.string();
		out << YAML::Key << "AssetRegistry" << YAML::Value << settings.AssetRegistryPath.string();
		out << YAML::Key << "StartingScene" << YAML::Value << settings.StartingScene;
		out << YAML::Key << "ScriptAssembly" << YAML::Value << settings.ScriptAssemblyPath.string();
		out << YAML::Key << "Renderer";
		out << YAML::BeginMap;

		out << YAML::Key << "AntiAliasing";
		out << YAML::BeginMap;
		out << YAML::Key << "Type" << YAML::Value << FromAATypeToString(renderer_settings.AntiAliasing.Type);
		out << YAML::Key << "PostProcessing" << YAML::Value << FromPostAATypeToString(renderer_settings.AntiAliasing.PostProcessing);
		out << YAML::Key << "Multiplier" << YAML::Value << renderer_settings.AntiAliasing.Multiplier;
		out << YAML::EndMap;

		out << YAML::EndMap;
		out << YAML::EndMap;

		std::filesystem::path filepath = settings.RootPath / std::filesystem::path(settings.ProjectName + file_extension);
		std::ofstream fout(filepath.string());
		HVE_CORE_ASSERT(fout, "Failed to open file for writing: {0}", filepath.string());

		fout << out.c_str();
		fout.close();
		HVE_CORE_ASSERT(!fout.fail(), "Failed to write data to file: {0}", filepath.string());
		HVE_CORE_TRACE_TAG("Project Serializer", "Project saved successfully to: {0}", filepath.string());

	}
	ProjectSettings ProjectSerializer::Deserializer(const std::filesystem::path& filepath)
	{
		YAML::Node config = YAML::LoadFile(filepath.string());
		ProjectSettings settings{};
		const std::filesystem::path root_path = filepath.parent_path();
		settings.ProjectName = config["Project"].as<std::string>();
		settings.RootPath = root_path;
		settings.AssetPath = std::filesystem::path(config["AssetPath"].as<std::string>());
		if (config["ScriptAssembly"])
		{
			settings.ScriptAssemblyPath = std::filesystem::path(config["ScriptAssembly"].as<std::string>());
		}
		if (config["AssetRegistry"])
		{
			settings.AssetRegistryPath = std::filesystem::path(config["AssetRegistry"].as<std::string>());
		}
		settings.StartingScene = config["StartingScene"].as<AssetHandle>(0);

		if (config["Renderer"])
		{
			if (config["Renderer"]["AntiAliasing"])
			{
				AntiAliasingSettings renderer_aa_settings{};
				renderer_aa_settings.Multiplier = config["Renderer"]["AntiAliasing"]["Multiplier"].as<int>();
				renderer_aa_settings.Type = FromStringToAAType(config["Renderer"]["AntiAliasing"]["Type"].as<std::string>("None"));
				renderer_aa_settings.PostProcessing = FromStringToPostAAType(config["Renderer"]["AntiAliasing"]["PostProcessing"].as<std::string>("None"));
				Renderer::Get()->SetAntiAliasing(renderer_aa_settings);
			}
		}

		return settings;
	}
	void ProjectSerializer::CreateScriptProject()
	{
		auto& project_settings = Project::GetActive()->GetSettings();

		std::filesystem::path script_project_path = project_settings.RootPath / "ScriptProject";
		std::filesystem::create_directory(script_project_path);

		// Find ScriptCore.dll path
		std::filesystem::path root_path = ROOT_PATH;
		root_path = root_path.parent_path();
		std::filesystem::path scriptcore_dll = root_path / "Editor/Resources/Scripts/ScriptCore.dll";

		// Create .csproj
		std::ofstream csproj(script_project_path / (project_settings.ProjectName + ".csproj"));
		csproj << "<Project Sdk=\"Microsoft.NET.Sdk\">\n";
		csproj << "  <PropertyGroup>\n";
		csproj << "    <TargetFramework>net10.0</TargetFramework>\n";
		csproj << "    <RootNamespace>" << project_settings.ProjectName << "</RootNamespace>\n";
		csproj << "    <AllowUnsafeBlocks>true</AllowUnsafeBlocks>\n";
		csproj << "    <OutputPath>../Binaries/</OutputPath>\n";
		csproj << "    <AppendTargetFrameworkToOutputPath>false</AppendTargetFrameworkToOutputPath>\n";
		csproj << "    <AppendRuntimeIdentifierToOutputPath>false</AppendRuntimeIdentifierToOutputPath>\n";
		csproj << "  </PropertyGroup>\n";
		csproj << "  <ItemGroup>\n";
		csproj << "    <Reference Include=\"ScriptCore\">\n";
		csproj << "      <HintPath>" << scriptcore_dll.string() << "</HintPath>\n";
		csproj << "    </Reference>\n";
		csproj << "  </ItemGroup>\n";
		csproj << "  <ItemGroup>\n";
		csproj << "    <Compile Include=\"../Assets/Scripts/**/*.cs\" />\n";
		csproj << "  </ItemGroup>\n";
		csproj << "</Project>\n";
		csproj.close();

		// Build
		CommandArgs args{};
		args.SleepUntilFinished = true;
		std::string command = "dotnet build \"" + (script_project_path / (project_settings.ProjectName + ".csproj")).string() + "\"";
		CommandLine::Create()->ExecuteCommand(command, args);

		project_settings.ScriptAssemblyPath = std::filesystem::path("Binaries/" + project_settings.ProjectName + ".dll");
	}
}
