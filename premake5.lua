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

project "helios"
    location "helios"
    kind "SharedLib"
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

    libdirs
    {
        "$(VULKAN_SDK)/Lib",
        "%{prj.name}/vendor/glfw/lib-vc2022"
    }

    links
    {
        "vulkan-1",
        "glfw3"
    }

    includedirs
    {
        "%{prj.name}/vendor/spdlog/include",
        "%{prj.name}/vendor/glfw/include",
        "%{prj.name}/vendor/glm",
        "%{prj.name}/vendor/stb",
        "$(VULKAN_SDK)/Include",
        "%{prj.name}/include"
    }

    filter "system:windows"
        cppdialect "C++17"
        staticruntime "On"
        systemversion "latest"

        defines
        {
            "HVE_PLATFORM_WINDOWS",
            "HVE_BUILD_DLL"
        }

        postbuildcommands
        {
            ("{COPY} %{cfg.buildtarget.relpath} ../bin/" .. outputdir .. "/Sandbox")
        }

    filter "configurations:Debug"
        defines "HVE_DEBUG"
        symbols "On"

    filter "configurations:Release"
        defines "HVE_RELEASE"
        optimize "On"

    filter "configurations:Dist"
        defines "HVE_DIST"
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
        "helios/include"
    }

    links
    {
        "helios"
    }

    filter "system:windows"
        cppdialect "C++17"
        staticruntime "On"
        systemversion "latest"

        defines
        {
            "HVE_PLATFORM_WINDOWS"
        }

    filter "configurations:Debug"
        defines "HVE_DEBUG"
        symbols "On"

    filter "configurations:Release"
        defines "HVE_RELEASE"
        optimize "On"

    filter "configurations:Dist"
        defines "HVE_DIST"
        optimize "On"


