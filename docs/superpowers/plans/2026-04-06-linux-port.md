# Helios Engine Linux Port — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the Helios game engine fully compile, link, and run on Linux with vendored dependencies.

**Architecture:** Fix all platform abstraction gaps (LinuxWindow, LinuxInput, LinuxCommandLine), update premake build system for Linux library paths/links, build nfd from source via premake, and create a vendor setup script for assimp and mono Linux static libraries.

**Tech Stack:** C++20, Premake5/gmake2, OpenGL (Glad), GLFW, ImGui, JoltPhysics, SoLoud (miniaudio), Mono, Assimp, NFD (GTK), Tracy

---

### Task 1: Fix EntryPoint.h — Linux excluded from compilation

**Files:**
- Modify: `Engine/src/Core/EntryPoint.h:3`

- [ ] **Step 1: Add PLATFORM_LINUX to the preprocessor guard**

```cpp
// Change line 3 from:
#if defined(PLATFORM_WINDOWS)
// To:
#if defined(PLATFORM_WINDOWS) || defined(PLATFORM_LINUX)
```

- [ ] **Step 2: Commit**
```bash
git add Engine/src/Core/EntryPoint.h
git commit -m "fix: allow Linux platform in EntryPoint.h"
```

---

### Task 2: Fix Assert.h and Base.h for Linux/GCC

**Files:**
- Modify: `Engine/src/Core/Base.h:10-22`
- Modify: `Engine/src/Core/Assert.h:6-12`

- [ ] **Step 1: Add GCC detection and fix THROW_NATIVE_ERROR fallback in Base.h**

In Base.h, add GCC detection after the existing compiler checks (line 10-14):

```cpp
#if defined(__clang__)
#define HVE_COMPILER_CLANG
#elif defined(__GNUC__)
#define HVE_COMPILER_GCC
#elif defined(_MSC_VER)
#define HVE_COMPILER_MSVC
#endif
```

Update HVE_FORCE_INLINE (line 16-22):

```cpp
#ifdef HVE_COMPILER_MSVC
#define HVE_FORCE_INLINE __forceinline
#elif defined(HVE_COMPILER_CLANG) || defined(HVE_COMPILER_GCC)
#define HVE_FORCE_INLINE __attribute__((always_inline)) inline
#else
#define HVE_FORCE_INLINE inline
#endif
```

- [ ] **Step 2: Fix HVE_DEBUG_BREAK in Assert.h for Linux/GCC**

Replace lines 6-12 with:

```cpp
#ifdef PLATFORM_WINDOWS
#define HVE_DEBUG_BREAK __debugbreak()
#elif defined(HVE_COMPILER_CLANG)
#define HVE_DEBUG_BREAK __builtin_debugtrap()
#elif defined(PLATFORM_LINUX)
#define HVE_DEBUG_BREAK __builtin_trap()
#else
#define HVE_DEBUG_BREAK
#endif
```

- [ ] **Step 3: Commit**
```bash
git add Engine/src/Core/Base.h Engine/src/Core/Assert.h
git commit -m "fix: add GCC compiler detection and Linux debug break support"
```

---

### Task 3: Update LinuxWindow to implement full Window interface

**Files:**
- Modify: `Engine/src/Platform/Linux/LinuxWindow.h`
- Modify: `Engine/src/Platform/Linux/LinuxWindow.cpp`

The Window base class (Core/Window.h) has pure virtual methods that LinuxWindow doesn't implement:
- `GetFullScreen()`, `GetFullScreenType()`, `GetMaximized()`, `GetVSync()`, `GetTitle()`
- `SetTitle()`, `SetFullScreen()`, `SetMaximized()`
- `IsKeyPressed()`, `SetKeyState()`, `ClearKeyStates()`

Also, LinuxWindow.cpp uses old log macros (CORE_ERROR, CORE_INFO, CORE_ASSERT) that don't exist anymore.

- [ ] **Step 1: Rewrite LinuxWindow.h to match WindowsWindow.h feature parity**

Port all the members and method declarations from WindowsWindow.h, adapted for Linux:

```cpp
#pragma once

#include "Core/Window.h"

#include <GLFW/glfw3.h>

namespace Engine
{
    class LinuxWindow : public Window
    {
    public:
        LinuxWindow(const WindowProps &props);
        virtual ~LinuxWindow();

        void OnUpdate() override;

        inline unsigned int GetWidth() const override { return m_Data.Width; }
        inline unsigned int GetHeight() const override { return m_Data.Height; }
        inline bool GetFullScreen() const override { return m_Data.Fullscreen; }
        inline FullscreenType GetFullScreenType() const override { return m_Data.FullscreenType; }
        inline bool GetMaximized() const override { return m_Data.ScreenMaximized; }
        inline bool GetVSync() const override { return m_Data.VSync; }
        inline std::string& GetTitle() override { return m_Data.Title; }
        void SetTitle(std::string& new_title) override;

        inline void SetEventCallback(const EventCallbackFn &callback) override { m_Data.EventCallback = callback; }

        inline void *GetNativeWindow() const override { return m_Window; }

        void SetFullScreen(bool fullscreen, FullscreenType type) override;
        void SetMaximized(bool maximized) override;

        bool IsKeyPressed(uint32_t key) override;
        void SetKeyState(uint32_t key, bool state) override;
        void ClearKeyStates() override;

    private:
        virtual void Init(const WindowProps &props);
        virtual void Shutdown();
        static void OnMaximize(GLFWwindow* window, int maximized);
        static void OnSizeChange(GLFWwindow* window, int width, int height);
        bool IsFullScreen();
        GLFWwindow *m_Window;

        struct WindowData
        {
            std::string Title;
            unsigned int Width, Height;
            bool VSync;
            bool ScreenMaximized;
            bool Fullscreen;
            FullscreenType FullscreenType;
            EventCallbackFn EventCallback;
        };

        WindowData m_Data;
        int XPos = 0, YPos = 0, PrevWidth, PrevHeight;
    };
}
```

- [ ] **Step 2: Rewrite LinuxWindow.cpp — full implementation matching WindowsWindow**

Port the full WindowsWindow.cpp implementation, fixing log macros to use `HVE_CORE_*` variants:
- `CORE_ERROR(...)` → `HVE_CORE_ERROR_TAG("LinuxWindow", ...)`
- `CORE_INFO(...)` → `HVE_CORE_TRACE_TAG("Window", ...)`
- `CORE_ASSERT(...)` → `HVE_CORE_ASSERT(...)`

Key areas: Init (with Resizable hint, VSync, Fullscreen, Maximized props), SetTitle, SetFullScreen (FULLSCREEN and BORDERLESS), SetMaximized, IsKeyPressed with s_KeyStates map, key state tracking in GLFW callbacks, OnMaximize/OnSizeChange static callbacks.

- [ ] **Step 3: Commit**
```bash
git add Engine/src/Platform/Linux/LinuxWindow.h Engine/src/Platform/Linux/LinuxWindow.cpp
git commit -m "feat: update LinuxWindow to full Window interface parity with WindowsWindow"
```

---

### Task 4: Update LinuxInput — add ClearKeyStatesImpl

**Files:**
- Modify: `Engine/src/Platform/Linux/LinuxInput.h`
- Modify: `Engine/src/Platform/Linux/LinuxInput.cpp`

- [ ] **Step 1: Add ClearKeyStatesImpl and b_IsLocked to LinuxInput.h**

```cpp
#pragma once
#include "Core/Input.h"

namespace Engine
{
    class LinuxInput : public Input
    {
    protected:
        bool IsKeyPressedImpl(int keycode) override;
        void SetLockMouseModeImpl(bool lock_mouse) override;
        bool IsMouseButtonPressedImpl(int button) override;
        float GetMouseXImpl() override;
        float GetMouseYImpl() override;
        std::pair<float, float> GetMousePositionImpl() override;
        void ClearKeyStatesImpl() override;

    private:
        bool b_IsLocked = false;
    };
}
```

- [ ] **Step 2: Update LinuxInput.cpp — use Window key state tracking, add ClearKeyStatesImpl**

Update IsKeyPressedImpl to use window key state tracking (matching WindowsInput):

```cpp
bool LinuxInput::IsKeyPressedImpl(int keycode)
{
    auto& window = Application::Get().GetWindow();
    auto native_window = static_cast<GLFWwindow *>(window.GetNativeWindow());
    auto state = glfwGetKey(native_window, keycode);
    return window.IsKeyPressed(keycode) || state == GLFW_REPEAT;
}
```

Update SetLockMouseModeImpl with the b_IsLocked guard:

```cpp
void LinuxInput::SetLockMouseModeImpl(bool lock_mouse)
{
    if (lock_mouse != b_IsLocked) {
        auto window = static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindow());
        glfwSetCursorPos(window, Application::Get().GetWindow().GetWidth() / 2, Application::Get().GetWindow().GetHeight() / 2);
        lock_mouse ? glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED) : glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        b_IsLocked = lock_mouse;
    }
}
```

Add ClearKeyStatesImpl:

```cpp
void LinuxInput::ClearKeyStatesImpl()
{
    Application::Get().GetWindow().ClearKeyStates();
}
```

- [ ] **Step 3: Commit**
```bash
git add Engine/src/Platform/Linux/LinuxInput.h Engine/src/Platform/Linux/LinuxInput.cpp
git commit -m "feat: update LinuxInput with ClearKeyStates and key state tracking"
```

---

### Task 5: Create LinuxCommandLine

**Files:**
- Create: `Engine/src/Platform/Linux/LinuxCommandLine.h`
- Create: `Engine/src/Platform/Linux/LinuxCommandLine.cpp`

- [ ] **Step 1: Create LinuxCommandLine.h**

```cpp
#pragma once

#include "Core/IO.h"

namespace Engine {
    class LinuxCommandLine : public CommandLine
    {
    public:
        void ExecuteCommand(std::string& command, CommandArgs& arguments) override;
    };
}
```

- [ ] **Step 2: Create LinuxCommandLine.cpp using fork/exec and waitpid**

Use POSIX fork/exec to implement the command execution. Include the `CommandLine::Create()` factory function for Linux.

Key implementation:
- fork() the process
- In child: chdir if UseAnotherWorkingDir, then execl("/bin/sh", "sh", "-c", command, nullptr)
- In parent: waitpid if SleepUntilFinished, check exit status
- Log with HVE_CORE_TRACE_TAG/HVE_CORE_ERROR_TAG

- [ ] **Step 3: Commit**
```bash
git add Engine/src/Platform/Linux/LinuxCommandLine.h Engine/src/Platform/Linux/LinuxCommandLine.cpp
git commit -m "feat: add LinuxCommandLine using POSIX fork/exec"
```

---

### Task 6: Fix ProjectSerializer.cpp for Linux

**Files:**
- Modify: `Engine/src/Project/ProjectSerializer.cpp:186-194`

- [ ] **Step 1: Fix the stray colon and add Linux premake path**

Replace the CreateScriptProject platform block (lines 186-194):

```cpp
#ifdef PLATFORM_WINDOWS
    std::filesystem::path premake_executable = root_path / std::filesystem::path("vendor/premake/bin/premake5.exe");
    std::string command = premake_executable.string() + " --file=" + project_settings.RootPath.string() + "/ScriptProject/premake5.lua" + " vs2022";
    std::replace(command.begin(), command.end(), '/', '\\');
#elif defined(PLATFORM_LINUX)
    std::filesystem::path premake_executable = root_path / std::filesystem::path("vendor/premake/premake5");
    std::string command = premake_executable.string() + " --file=" + project_settings.RootPath.string() + "/ScriptProject/premake5.lua" + " gmake2";
#endif
```

- [ ] **Step 2: Commit**
```bash
git add Engine/src/Project/ProjectSerializer.cpp
git commit -m "fix: add Linux premake path in ProjectSerializer and fix stray colon"
```

---

### Task 7: Create NFD premake5.lua for building from source on Linux

**Files:**
- Create: `Engine/vendor/nativefiledialog-extended/premake5.lua`

- [ ] **Step 1: Create premake5.lua that builds nfd_gtk.cpp on Linux, nfd_win.cpp on Windows**

```lua
project "nfd"
    kind "StaticLib"
    language "C++"
    staticruntime "off"

    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

    includedirs { "src/include" }

    filter "system:windows"
        systemversion "latest"
        files { "src/nfd_win.cpp" }

    filter "system:linux"
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
```

- [ ] **Step 2: Commit**
```bash
git add Engine/vendor/nativefiledialog-extended/premake5.lua
git commit -m "feat: add nfd premake5.lua to build from source on Linux and Windows"
```

---

### Task 8: Fix Dependencies.lua for Linux

**Files:**
- Modify: `Dependencies.lua`

- [ ] **Step 1: Add platform-conditional mono library path and assimp library**

```lua
IncludeDir = {}
IncludeDir["GLFW"] = "%{wks.location}/Engine/vendor/GLFW/include"
IncludeDir["Glad"] = "%{wks.location}/Engine/vendor/Glad/include"
IncludeDir["ImGui"] = "%{wks.location}/Engine/vendor/imgui"
IncludeDir["glm"] = "%{wks.location}/Engine/vendor/glm"
IncludeDir["Jolt"] = "%{wks.location}/Engine/vendor/JoltPhysics/JoltPhysics"
IncludeDir["Tracy"] = "%{wks.location}/Engine/vendor/tracy/tracy/public"
IncludeDir["Assimp"] = "%{wks.location}/Engine/vendor/assimp/include"
IncludeDir["YamlCpp"] = "%{wks.location}/Engine/vendor/yaml-cpp/include"
IncludeDir["mono"] = "%{wks.location}/Engine/vendor/mono/include"
IncludeDir["FileWatcher"] = "%{wks.location}/Engine/vendor/filewatch/include"
IncludeDir["nfd"] = "%{wks.location}/Engine/vendor/nativefiledialog-extended/src/include"

rootPath = path.getabsolute(".")

LibraryDir = {}
LibraryDir["mono_win"] = "%{wks.location}/Engine/vendor/mono/lib/%{cfg.buildcfg}"
LibraryDir["mono_linux"] = "%{wks.location}/Engine/vendor/mono/lib/linux"
LibraryDir["assimp_linux"] = "%{wks.location}/Engine/vendor/assimp/lib/linux-x64"

Library = {}
Library["Jolt"] = "JoltPhysics"
Library["Tracy"] = "Tracy"
Library["mono_win"] = "%{LibraryDir.mono_win}/libmono-static-sgen.lib"
Library["mono_linux"] = "monosgen-2.0"

Binaries = {}

-- Windows platform specific libraries
Library["WinSock"] = "Ws2_32.lib"
Library["WinMM"] = "Winmm.lib"
Library["WinVersion"] = "Version.lib"
Library["BCrypt"] = "Bcrypt.lib"
Library["DebugHelp"] = "Dbghelp.lib"
```

- [ ] **Step 2: Commit**
```bash
git add Dependencies.lua
git commit -m "feat: add Linux library paths and mono/assimp config to Dependencies.lua"
```

---

### Task 9: Fix Engine premake5.lua for Linux

**Files:**
- Modify: `Engine/premake5.lua`

- [ ] **Step 1: Update the Linux filter with proper libdirs, links, and system libraries**

The Linux filter (lines 109-126) needs:
- libdirs for Linux vendor libraries (assimp, mono)
- Links: GL, GLFW, assimp, mono, nfd, pthread, dl, m, rt, X11, GTK (via pkg-config)
- Replace hardcoded Windows assimp/nfd/mono links with platform conditionals
- Add nfd as a project dependency

Key changes:
- Move `assimp-vc143-mt.lib` and `nfd.lib` and `%{Library.mono}` into the Windows filter
- In the Linux filter, link against: `GL`, `assimp`, `nfd`, `monosgen-2.0`, `pthread`, `dl`, `m`, `rt`
- Add nfd buildoptions for GTK pkg-config on Linux
- Add libdirs for Linux vendor paths
- Remove `vendor/GLFW/lib-vc2022` from the shared libdirs (it's Windows-only), put it in Windows filter

- [ ] **Step 2: Add nfd to the dependency group in main premake5.lua**

Add `include "Engine/vendor/nativefiledialog-extended"` in the Dependencies group.

- [ ] **Step 3: Commit**
```bash
git add Engine/premake5.lua premake5.lua
git commit -m "fix: update Engine premake5.lua with proper Linux library linking"
```

---

### Task 10: Fix Editor and EditorLauncher premake5.lua for Linux

**Files:**
- Modify: `Editor/premake5.lua`
- Modify: `EditorLauncher/premake5.lua`

- [ ] **Step 1: Fix Editor premake5.lua**

- Move the assimp DLL postbuild command into a Windows filter
- Add Linux links: `GL`, `pthread`, `dl`, `m`
- Remove Linux postbuild (no DLL to copy)

- [ ] **Step 2: Fix EditorLauncher premake5.lua**

- Fix `EDITOR_EXECUTABLE_PATH` to not have `.exe` on Linux
- Move `assimp-vc143-mt.dll` postbuild into Windows filter
- Move `nfd.lib` link into Windows filter, link `nfd` project on Linux
- Add Linux links: `GL`, `pthread`, `dl`, `m`
- Add GTK link flags on Linux for nfd

- [ ] **Step 3: Commit**
```bash
git add Editor/premake5.lua EditorLauncher/premake5.lua
git commit -m "fix: update Editor/EditorLauncher premake files for Linux"
```

---

### Task 11: Create Linux vendor setup script

**Files:**
- Create: `setup_linux_vendor.sh`

- [ ] **Step 1: Create script that builds/installs assimp and mono into vendor dirs**

The script will:
1. Check for required system packages (cmake, gcc, make, pkg-config, gtk3-devel)
2. Build assimp from source (clone into /tmp, cmake, make, copy .a to vendor/assimp/lib/linux-x64/)
3. Build mono from source or download tarball, build static lib, copy to vendor/mono/lib/linux/
4. Make the premake5 binary executable

- [ ] **Step 2: Commit**
```bash
git add setup_linux_vendor.sh
git commit -m "feat: add Linux vendor dependency setup script"
```

---

### Task 12: Fix generate_linux_projects.sh

**Files:**
- Modify: `generate_linux_projects.sh`

- [ ] **Step 1: Update VS Code config paths and C++ standard**

- Fix the launch.json program path to match actual Editor output dir
- Update c_cpp_properties.json cppStandard to `c++20`
- Remove JSON comments (invalid JSON syntax with `#`)
- Remove trailing commas in JSON arrays

- [ ] **Step 2: Commit**
```bash
git add generate_linux_projects.sh
git commit -m "fix: update generate_linux_projects.sh with correct paths and C++20"
```
