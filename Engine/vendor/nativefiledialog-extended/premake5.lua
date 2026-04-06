project "nfd"
    kind "StaticLib"
    language "C++"
    cppdialect "C++17"
    staticruntime "off"

    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

    includedirs { "src/include" }

    filter "system:windows"
        systemversion "latest"
        files { "src/nfd_win.cpp" }

    filter "system:linux"
        pic "On"
        files { "src/nfd_gtk.cpp" }
        buildoptions { "`pkg-config --cflags gtk+-3.0`" }

    filter "configurations:Debug"
        runtime "Debug"
        symbols "on"

    filter "configurations:Release"
        runtime "Release"
        optimize "on"

    filter "configurations:Dist"
        runtime "Release"
        optimize "on"
