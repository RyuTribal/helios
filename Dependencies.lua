IncludeDir = {}
IncludeDir["GLFW"] = "%{wks.location}/Engine/vendor/GLFW/include"
IncludeDir["ImGui"] = "%{wks.location}/Engine/vendor/imgui"
IncludeDir["glm"] = "%{wks.location}/Engine/vendor/glm"
IncludeDir["Jolt"] = "%{wks.location}/Engine/vendor/JoltPhysics/JoltPhysics"
IncludeDir["Tracy"] = "%{wks.location}/Engine/vendor/tracy/tracy/public"
IncludeDir["Assimp"] = "%{wks.location}/Engine/vendor/assimp/include"
IncludeDir["YamlCpp"] = "%{wks.location}/Engine/vendor/yaml-cpp/include"
IncludeDir["nethost"] = "%{wks.location}/Engine/vendor/dotnet/include"
IncludeDir["FileWatcher"] = "%{wks.location}/Engine/vendor/filewatch/include"
IncludeDir["nfd"] = "%{wks.location}/Engine/vendor/nativefiledialog-extended/src/include"
IncludeDir["VulkanSDK"] = "/usr/include"
IncludeDir["vma"] = "%{wks.location}/Engine/vendor/vma"
IncludeDir["vkbootstrap"] = "%{wks.location}/Engine/vendor/vk-bootstrap"

rootPath = path.getabsolute(".")

LibraryDir = {}
LibraryDir["assimp_linux"] = "%{wks.location}/Engine/vendor/assimp/lib/linux-x64"

Library = {}
Library["Jolt"] = "JoltPhysics";
Library["Tracy"] = "Tracy";

Binaries = {}

-- Windows platform specific libraries
Library["WinSock"] = "Ws2_32.lib"
Library["WinMM"] = "Winmm.lib"
Library["WinVersion"] = "Version.lib"
Library["BCrypt"] = "Bcrypt.lib"
Library["DebugHelp"] = "Dbghelp.lib"
