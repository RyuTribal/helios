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

include "helios/vendor/GLFW"
include "helios/vendor/Glad"

project "helios"
    location "helios"
    kind "SharedLib"
    language "C++"

    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

    pchheader "hvepch.h"
    pchsource "helios/src/hvepch.cpp"

    files
    {
        "%{prj.name}/src/**.h",
        "%{prj.name}/src/**.hpp",
        "%{prj.name}/src/**.cpp",
        "%{prj.name}/src/**.c"
    }

    libdirs
    {
        "%{prj.name}/vendor/glfw/lib-vc2022"
    }

    links
    {
        "GLFW",
        "Glad",
        "opengl32.lib"
    }

    includedirs
    {
        "%{prj.name}/vendor/stb",
        "$(VULKAN_SDK)/Include",
        "%{prj.name}/vendor/spdlog/include",
        "%{IncludeDirs.GLFW}",
        "%{IncludeDirs.Glad}",
        "%{prj.name}/vendor/glm",
        "%{prj.name}/src"
    }

    filter "system:windows"
        cppdialect "C++17"
        staticruntime "Off"
        systemversion "latest"

        defines
        {
            "HVE_PLATFORM_WINDOWS",
            "HVE_BUILD_DLL",
            "GLFW_INCLUDE_NONE"
        }

        postbuildcommands
        {
            ("{COPY} %{cfg.buildtarget.relpath} ../bin/" .. outputdir .. "/Sandbox")
        }

    filter "configurations:Debug"
        defines "HVE_DEBUG"
        buildoptions "/MDd"
        symbols "On"

    filter "configurations:Release"
        defines "HVE_RELEASE"
        buildoptions "/MD"
        optimize "On"

    filter "configurations:Dist"
        defines "HVE_DIST"
        buildoptions "/MD"
        optimize "On"

project "Sandbox"
    location "Sandbox"
    kind "ConsoleApp"
    language "C++"
    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("bin-int/" .. outputdir .. "/%{prj.name}")
    files
    {
        "%{prj.name}/**.h",
        "%{prj.name}/**.hpp",
        "%{prj.name}/**.cpp",
        "%{prj.name}/**.c"
    }

    includedirs
    {
        "helios/vendor/spdlog/include",
        "helios/src"
    }

    links
    {
        "helios"
    }

    filter "system:windows"
        cppdialect "C++17"
        staticruntime "Off"
        systemversion "latest"

        defines
        {
            "HVE_PLATFORM_WINDOWS",
            
        }

    filter "configurations:Debug"
        defines "HVE_DEBUG"
        buildoptions "/MDd"
        symbols "On"

    filter "configurations:Release"
        defines "HVE_RELEASE"
        buildoptions "/MD"
        optimize "On"

    filter "configurations:Dist"
        defines "HVE_DIST"
        buildoptions "/MD"
        optimize "On"


