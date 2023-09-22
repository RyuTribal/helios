workspace "helios"
    architecture "x64"
    startproject "Sandbox"

    configurations
    {
        "Debug",
        "Release",
        "Dist"
    }

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

IncludeDirs = {}
IncludeDirs["GLFW"] = "helios/vendor/GLFW/include"
IncludeDirs["Glad"] = "helios/vendor/Glad/include"
IncludeDirs["ImGui"] = "helios/vendor/imgui"
IncludeDirs["glm"] = "helios/vendor/glm"

group "Dependencies"
    include "helios/vendor/GLFW"
    include "helios/vendor/Glad"
    include "helios/vendor/imgui"

group ""

group "Core"
project "helios"
    location "helios"
    kind "StaticLib"
    language "C++"
    cppdialect "C++17"
    staticruntime "on"

    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

    pchheader "hvepch.h"
    pchsource "helios/src/hvepch.cpp"

    files
    {
        "%{prj.name}/src/**.h",
        "%{prj.name}/src/**.hpp",
        "%{prj.name}/src/**.cpp",
        "%{prj.name}/src/**.c",
        "%{prj.name}/vendor/glm/glm/**.hpp",
        "%{prj.name}/vendor/glm/glm/**.inl"
    }

    libdirs
    {
        "%{prj.name}/vendor/glfw/lib-vc2022"
    }

    links
    {
        "GLFW",
        "Glad",
        "ImGui",
        "opengl32.lib"
    }

    defines
    {
        "_CRT_SECURE_NO_WARNINGS"
    }

    includedirs
    {
        "%{prj.name}/vendor/stb",
        "$(VULKAN_SDK)/Include",
        "%{prj.name}/vendor/spdlog/include",
        "%{IncludeDirs.GLFW}",
        "%{IncludeDirs.Glad}",
        "%{IncludeDirs.ImGui}",
        "%{IncludeDirs.glm}",
        "%{prj.name}/src"
    }

    filter "system:windows"
        systemversion "latest"

        defines
        {
            "HVE_PLATFORM_WINDOWS",
            "HVE_BUILD_DLL",
            "GLFW_INCLUDE_NONE"
        }

    filter "configurations:Debug"
        defines "HVE_DEBUG"
        runtime "Debug"
        symbols "on"

    filter "configurations:Release"
        defines "HVE_RELEASE"
        runtime "Release"
        optimize "on"

    filter "configurations:Dist"
        defines "HVE_DIST"
        runtime "Release"
        optimize "on"

group ""

group "Misc"

project "Sandbox"
    location "Sandbox"
    kind "ConsoleApp"
    staticruntime "on"
    language "C++"
    cppdialect "C++17"
    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("bin-int/" .. outputdir .. "/%{prj.name}")
    files
    {
        "%{prj.name}/**.h",
        "%{prj.name}/**.hpp",
        "%{prj.name}/**.cpp",
        "%{prj.name}/**.c",
    }

    includedirs
    {
        "helios/vendor/spdlog/include",
        "helios/src",
        "${IncludeDirs.glm}"
    }

    links
    {
        "helios"
    }

    filter "system:windows"
        systemversion "latest"

        defines
        {
            "HVE_PLATFORM_WINDOWS",
            
        }

    filter "configurations:Debug"
        defines "HVE_DEBUG"
        runtime "Debug"
        symbols "on"

    filter "configurations:Release"
        defines "HVE_RELEASE"
        runtime "Release"
        optimize "on"

    filter "configurations:Dist"
        defines "HVE_DIST"
        runtime "Release"
        optimize "on"

