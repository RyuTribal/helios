@echo off
setlocal enableextensions
cd /d "%~dp0"

set "SCRIPT_DIR=%~dp0"
set "SCRIPT_DIR=%SCRIPT_DIR:~0,-1%"
set "VENDOR_DIR=%SCRIPT_DIR%\Engine\vendor"

echo =========================================
echo   Helios Project Setup (Windows)
echo =========================================
echo.

:: ─── Check prerequisites ─────────────────────────────────────────────────────
echo [INFO] Checking prerequisites...

where cmake >nul 2>&1 || (echo [ERROR] cmake not found. Install from https://cmake.org & goto :error)
where git >nul 2>&1 || (echo [ERROR] git not found. Install from https://git-scm.com & goto :error)
where dotnet >nul 2>&1 || (echo [ERROR] dotnet not found. Install .NET 10 SDK from https://dot.net/download & goto :error)
where curl >nul 2>&1 || (echo [ERROR] curl not found. & goto :error)
where glslc >nul 2>&1 || (echo [ERROR] glslc not found. Install Vulkan SDK from https://vulkan.lunarg.com/sdk/home & goto :error)

echo [INFO] All prerequisites found.
echo.

:: ─── Build Assimp (if needed) ────────────────────────────────────────────────
:: Windows uses the prebuilt assimp libs that are already in vendor/assimp/lib/x64/
:: Only need to check they exist
if exist "%VENDOR_DIR%\assimp\lib\x64\assimp-vc143-mt.lib" (
    echo [INFO] Assimp Windows library already exists, skipping.
) else (
    echo [WARN] Assimp Windows library not found at vendor\assimp\lib\x64\
    echo [WARN] The prebuilt Windows libs should be checked into the repo.
    echo [WARN] Build may fail at link time for assimp.
)
echo.

:: ─── Download .NET 10 Runtime (if needed) ────────────────────────────────────
set "DOTNET_DIR=%VENDOR_DIR%\dotnet"

if exist "%DOTNET_DIR%\shared\Microsoft.NETCore.App" (
    echo [INFO] .NET 10 runtime already present, skipping.
) else (
    echo [INFO] Downloading .NET 10 runtime...
    if not exist "%DOTNET_DIR%" mkdir "%DOTNET_DIR%"

    :: Download dotnet-install.ps1
    powershell -NoProfile -ExecutionPolicy Bypass -Command ^
        "Invoke-WebRequest -Uri 'https://dot.net/v1/dotnet-install.ps1' -OutFile '%TEMP%\dotnet-install.ps1'"

    :: Run it to install just the runtime
    powershell -NoProfile -ExecutionPolicy Bypass -Command ^
        "& '%TEMP%\dotnet-install.ps1' -Channel 10.0 -Runtime dotnet -InstallDir '%DOTNET_DIR%'"

    if not exist "%DOTNET_DIR%\shared\Microsoft.NETCore.App" (
        echo [ERROR] Failed to download .NET 10 runtime
        goto :error
    )
    echo [INFO] .NET 10 runtime installed.
)
echo.

:: ─── Download hostfxr headers (if needed) ────────────────────────────────────
set "DOTNET_INCLUDE=%DOTNET_DIR%\include"

if exist "%DOTNET_INCLUDE%\hostfxr.h" (
    :: Check if it's a real header or just a placeholder
    for %%A in ("%DOTNET_INCLUDE%\hostfxr.h") do (
        if %%~zA GTR 200 (
            echo [INFO] hostfxr headers already present, skipping.
            goto :skip_headers
        )
    )
)

echo [INFO] Downloading hostfxr headers...
if not exist "%DOTNET_INCLUDE%" mkdir "%DOTNET_INCLUDE%"

set "HEADERS_BASE=https://raw.githubusercontent.com/dotnet/runtime/main/src/native/corehost"
curl -sSL "%HEADERS_BASE%/hostfxr.h" -o "%DOTNET_INCLUDE%\hostfxr.h"
curl -sSL "%HEADERS_BASE%/coreclr_delegates.h" -o "%DOTNET_INCLUDE%\coreclr_delegates.h"
curl -sSL "%HEADERS_BASE%/nethost/nethost.h" -o "%DOTNET_INCLUDE%\nethost.h"
echo [INFO] hostfxr headers downloaded.

:skip_headers
echo.

:: ─── Compile shaders to SPIR-V ───────────────────────────────────────────────
echo [INFO] Compiling shaders to SPIR-V...
for %%f in ("%SCRIPT_DIR%\Editor\Resources\Shaders\*.vert" "%SCRIPT_DIR%\Editor\Resources\Shaders\*.frag" "%SCRIPT_DIR%\Editor\Resources\Shaders\*.comp") do (
    if exist "%%f" (
        echo   Compiling: %%~nxf
        glslc "%%f" -o "%%f.spv" || goto :error
    )
)
for %%f in ("%SCRIPT_DIR%\Editor\Resources\Shaders\*.geo") do (
    if exist "%%f" (
        echo   Compiling: %%~nxf
        glslc -fshader-stage=geometry "%%f" -o "%%f.spv" || goto :error
    )
)
echo [INFO] All shaders compiled.
echo.

:: ─── Generate VS2022 projects ────────────────────────────────────────────────
echo [INFO] Generating Visual Studio 2022 projects...
call "%SCRIPT_DIR%\vendor\premake\bin\premake5.exe" vs2022

echo.
echo =========================================
echo   Project setup complete!
echo =========================================
echo.
echo To build: Open Helios.sln in Visual Studio 2022
echo           or: dotnet build ScriptCore\ScriptCore.csproj ^& msbuild Helios.sln
echo.

pause
exit /b 0

:error
echo.
echo [ERROR] Setup failed. Fix the issues above and re-run.
pause
exit /b 1
