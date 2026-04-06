project "ScriptCore"
    kind "SharedLib"
    staticruntime "off"
    language "C#"
    dotnetframework "4.7.2"
    targetdir (rootPath .. "/Editor/Resources/Scripts")
    objdir (rootPath .. "/Editor/Resources/Scripts/Intermediates")
    namespace "Helios"

    -- Remove workspace-level defines that are invalid for C#
    removedefines {
        "_CRT_SECURE_NO_WARNINGS",
        "_SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING",
        "TRACY_ENABLE",
        "TRACY_ON_DEMAND",
        "TRACY_CALLSTACK=10",
    }

    links {
        rootPath .. "/Editor/mono/lib/mono/4.5/System.Core.dll",
        rootPath .. "/Editor/mono/lib/mono/4.5/System.Numerics.dll",
        rootPath .. "/Editor/mono/lib/mono/4.5/System.Numerics.Vectors.dll"
    }

    files
    {
        "Source/**.cs",
        "Properties/**.cs",
    }

    filter "configurations:Debug"
        optimize "Off"
        symbols "Default"

    filter "configurations:Release"
        optimize "On"
        symbols "Default"

    filter "configurations:Dist"
        optimize "Full"
        symbols "Off"
