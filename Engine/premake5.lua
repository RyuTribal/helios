project "Engine"
    kind "StaticLib"
    staticruntime "off"
    targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")
	objdir ("%{wks.location}/bin-int/" .. outputdir .. "/%{prj.name}")

    pchheader "pch.h"
    pchsource "src/pch.cpp"

    files
    {
        "src/**.h",
        "src/**.hpp",
        "src/**.cpp",
        "src/**.c",
        "vendor/glm/glm/**.hpp",
        "vendor/glm/glm/**.inl",
        "vendor/yaml-cpp/src/**.cpp",
        "vendor/yaml-cpp/src/**.h",
        "vendor/yaml-cpp/include/**.h",
        "vendor/SoLoud/**.cpp",
        "vendor/SoLoud/**.h",
        "vendor/SoLoud/**.c",
        "vendor/vk-bootstrap/VkBootstrap.cpp",

    }

    removefiles
    {
        "src/Platform/**"
    }

    links
    {
        "GLFW",
        "Glad",
        "ImGui",
        "JoltPhysics",
        "%{Library.Tracy}",
        "nfd",
    }

    defines
    {
        "_CRT_SECURE_NO_WARNINGS",
        "ROOT_PATH=\"" .. rootPath .. "/" .. "%{prj.name}\"",
        "JPH_USE_LZCNT",
        "JPH_USE_TZCNT",
        "JPH_USE_FMADD",
        "YAML_CPP_STATIC_DEFINE"
    }

    includedirs
    {
        "vendor/spdlog/include",
        "vendor/stb",
        "vendor/",
        "vendor/SoLoud",
        "%{IncludeDir.GLFW}",
        "%{IncludeDir.Glad}",
        "%{IncludeDir.ImGui}",
        "%{IncludeDir.glm}",
        "%{IncludeDir.Jolt}",
        "%{IncludeDir.Jolt}/Jolt",
        "%{IncludeDir.Tracy}",
        "%{IncludeDir.Assimp}",
        "%{IncludeDir.YamlCpp}",
        "%{IncludeDir.nfd}",
        "%{IncludeDir.VulkanSDK}",
        "%{IncludeDir.vma}",
        "%{IncludeDir.vkbootstrap}",
        "vendor/filewatch/include",
        "%{IncludeDir.nethost}",
        "src/",

    }

    flags { "NoPCH" }

    filter "system:windows"
        systemversion "latest"
        defines
        {
            "PLATFORM_WINDOWS",
            "BUILD_DLL",
            "GLFW_INCLUDE_NONE"
        }

        libdirs
        {
            "vendor/GLFW/lib-vc2022",
            "vendor/assimp/lib/x64",
        }

        links
        {
            "%{Library.WinSock}",
			"%{Library.WinMM}",
			"%{Library.WinVersion}",
			"%{Library.BCrypt}",
            "%{Library.DebugHelp}",
            "opengl32.lib",
            "assimp-vc143-mt.lib",
        }

        files
        {
            "src/Platform/Windows/**.cpp",
            "src/Platform/Windows/**.h",
        }

    filter "system:linux"
        systemversion "latest"
        pic "On"
        buildoptions { "-Wno-changes-meaning" }
        defines
        {
            "PLATFORM_LINUX",
            "BUILD_DLL",
            "GLFW_INCLUDE_NONE"
        }

        libdirs
        {
            "%{LibraryDir.assimp_linux}",
        }

        links
        {
            "vulkan",
            "GL",
            "X11",
            "assimp",
            "pthread",
            "dl",
            "m",
            "rt",
        }

        linkoptions { "`pkg-config --libs gtk+-3.0`" }

        files
        {
            "src/Platform/Linux/**.cpp",
            "src/Platform/Linux/**.h",
        }

    filter "configurations:Debug"
    defines {
            "DEBUG",
            "JPH_ENABLE_ASSERTS",
            "JPH_DEBUG_RENDERER",
            "JPH_FLOATING_POINT_EXCEPTIONS_ENABLED",
            -- "JPH_EXTERNAL_PROFILE"
        }
        runtime "Debug"
        symbols "on"

    filter "configurations:Release"
        defines {
            "RELEASE",
            "JPH_ENABLE_ASSERTS",
            "JPH_DEBUG_RENDERER",
            "JPH_FLOATING_POINT_EXCEPTIONS_ENABLED",
            "JPH_EXTERNAL_PROFILE"
        }
        runtime "Release"
        optimize "on"

    filter "configurations:Dist"
        defines "DIST"
        runtime "Release"
        optimize "on"
