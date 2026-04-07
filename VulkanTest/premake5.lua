project "VulkanTest"
    kind "ConsoleApp"
    staticruntime "off"

    targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")
    objdir ("%{wks.location}/bin-int/" .. outputdir .. "/%{prj.name}")

    files { "src/**.h", "src/**.cpp" }

    includedirs {
        "%{wks.location}/Engine/vendor/spdlog/include",
        "%{IncludeDir.glm}",
        "%{IncludeDir.GLFW}",
        "%{IncludeDir.VulkanSDK}",
        "%{IncludeDir.vma}",
        "%{IncludeDir.vkbootstrap}",
        "%{IncludeDir.Tracy}",
        "%{wks.location}/Engine/vendor",
        "%{wks.location}/Engine/src",
    }

    links {
        "Engine",
        "GLFW",
        "ImGui",
        "JoltPhysics",
        "%{Library.Tracy}",
        "nfd",
    }

    defines {
        "ROOT_PATH=\"" .. rootPath .. "/" .. "%{prj.name}\"",
        "JPH_USE_LZCNT",
        "JPH_USE_TZCNT",
        "JPH_USE_FMADD",
    }

    filter "system:linux"
        systemversion "latest"
        pic "On"
        buildoptions { "-Wno-changes-meaning" }
        defines { "PLATFORM_LINUX" }
        libdirs { "%{LibraryDir.assimp_linux}" }
        links { "GL", "X11", "vulkan", "assimp", "pthread", "dl", "m", "rt" }
        linkgroups "On"
        linkoptions { "`pkg-config --libs gtk+-3.0`" }

    filter "configurations:Debug"
        defines { "DEBUG", "JPH_ENABLE_ASSERTS", "JPH_DEBUG_RENDERER", "JPH_FLOATING_POINT_EXCEPTIONS_ENABLED" }
        runtime "Debug"
        symbols "on"

    filter "configurations:Release"
        defines { "RELEASE", "JPH_ENABLE_ASSERTS", "JPH_DEBUG_RENDERER", "JPH_FLOATING_POINT_EXCEPTIONS_ENABLED", "JPH_EXTERNAL_PROFILE" }
        runtime "Release"
        optimize "on"

    filter "configurations:Dist"
        defines "DIST"
        runtime "Release"
        optimize "on"
