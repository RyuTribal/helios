project "EditorLauncher"
    kind "ConsoleApp"
    staticruntime "off"

    targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")
	objdir ("%{wks.location}/bin-int/" .. outputdir .. "/%{prj.name}")
    files
    {
        "src/**.h",
        "src/**.hpp",
        "src/**.cpp",
        "src/**.c",
    }

    includedirs
    {
        "%{wks.location}/Engine/vendor/spdlog/include",
        "%{IncludeDir.glm}",
        "%{IncludeDir.Jolt}",
        "%{IncludeDir.Jolt}/Jolt",
        "%{IncludeDir.Tracy}",
        "%{IncludeDir.Assimp}",
        "%{wks.location}/Engine/vendor",
        "%{wks.location}/Engine/src",
        "%{IncludeDir.nfd}",
        "%{wks.location}/Engine/vendor/SoLoud",
    }

    links
    {
        "Engine",
        "GLFW",
        "ImGui",
        "JoltPhysics",
        "%{Library.Tracy}",
        "nfd",
    }

    defines
    {
        "ROOT_PATH=\"" .. rootPath .. "/" .. "%{prj.name}\"",
        "JPH_USE_LZCNT",
        "JPH_USE_TZCNT",
        "JPH_USE_FMADD"
    }

    filter "system:windows"
        systemversion "latest"
        defines
        {
            "PLATFORM_WINDOWS",
            'EDITOR_EXECUTABLE_PATH="'.. rootPath ..'/bin/' .. outputdir .. '/Editor/Editor.exe"',
            'EDITOR_WORKING_DIRECTORY="'.. rootPath ..'/Editor/"',
        }
        links
        {
            "opengl32.lib"
        }
        postbuildcommands
        {
            '{COPY} "%{wks.location}/Engine/vendor/assimp/shared/x64/assimp-vc143-mt.dll" "%{cfg.targetdir}"'
        }

    filter "system:linux"
        systemversion "latest"
        pic "On"
        buildoptions { "-Wno-changes-meaning" }
        defines
        {
            "PLATFORM_LINUX",
            'EDITOR_EXECUTABLE_PATH="'.. rootPath ..'/bin/' .. outputdir .. '/Editor/Editor"',
            'EDITOR_WORKING_DIRECTORY="'.. rootPath ..'/Editor/"',
        }

        libdirs
        {
            "%{LibraryDir.assimp_linux}",
        }

        links
        {
            "vulkan",
            "X11",
            "assimp",
            "pthread",
            "dl",
            "m",
            "rt",
        }
        linkgroups "On"
        linkoptions { "`pkg-config --libs gtk+-3.0`" }

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
