#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VENDOR_DIR="$SCRIPT_DIR/Engine/vendor"
BUILD_DIR="/tmp/helios-vendor-build"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

log_info()  { echo -e "${GREEN}[INFO]${NC} $1"; }
log_warn()  { echo -e "${YELLOW}[WARN]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }

check_command() {
    if ! command -v "$1" &> /dev/null; then
        log_error "$1 is required but not installed."
        return 1
    fi
}

# ─── Check prerequisites ──────────────────────────────────────────────────────
log_info "Checking prerequisites..."

MISSING=0
for cmd in cmake make gcc g++ pkg-config git curl dotnet; do
    if ! check_command "$cmd"; then
        MISSING=1
    fi
done

if ! pkg-config --exists gtk+-3.0 2>/dev/null; then
    log_error "GTK3 development files not found. Install gtk3 (e.g. 'sudo pacman -S gtk3' on Arch, 'sudo apt install libgtk-3-dev' on Debian/Ubuntu)"
    MISSING=1
fi

if [ "$MISSING" -eq 1 ]; then
    log_error "Missing prerequisites. Please install them and re-run."
    log_info "On Arch: sudo pacman -S cmake make gcc pkg-config git curl gtk3 dotnet-sdk-10.0"
    log_info "On Ubuntu: sudo apt install cmake make gcc g++ pkg-config git curl libgtk-3-dev dotnet-sdk-10.0"
    exit 1
fi

log_info "All prerequisites found."

# ─── Build Assimp (if needed) ─────────────────────────────────────────────────
ASSIMP_LIB_DIR="$VENDOR_DIR/assimp/lib/linux-x64"
if [ -f "$ASSIMP_LIB_DIR/libassimp.a" ]; then
    log_info "Assimp Linux library already exists, skipping."
else
    log_info "Building Assimp from source..."
    mkdir -p "$BUILD_DIR"
    ASSIMP_BUILD="$BUILD_DIR/assimp"
    rm -rf "$ASSIMP_BUILD"

    git clone --depth 1 --branch v5.4.3 https://github.com/assimp/assimp.git "$ASSIMP_BUILD"

    cmake -S "$ASSIMP_BUILD" -B "$ASSIMP_BUILD/build" \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=OFF \
        -DASSIMP_BUILD_TESTS=OFF \
        -DASSIMP_INSTALL=OFF \
        -DASSIMP_BUILD_ASSIMP_TOOLS=OFF \
        -DASSIMP_BUILD_ZLIB=ON \
        -DASSIMP_WARNINGS_AS_ERRORS=OFF \
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON

    cmake --build "$ASSIMP_BUILD/build" -j "$(nproc)"

    mkdir -p "$ASSIMP_LIB_DIR"
    cp "$ASSIMP_BUILD/build/lib/libassimp.a" "$ASSIMP_LIB_DIR/"
    [ -f "$ASSIMP_BUILD/build/contrib/zlib/libzlibstatic.a" ] && cp "$ASSIMP_BUILD/build/contrib/zlib/libzlibstatic.a" "$ASSIMP_LIB_DIR/"

    rm -rf "$ASSIMP_BUILD"
    log_info "Assimp built and installed."
fi

# ─── Download .NET 10 Runtime (if needed) ─────────────────────────────────────
DOTNET_DIR="$VENDOR_DIR/dotnet"
if [ -d "$DOTNET_DIR/shared/Microsoft.NETCore.App" ]; then
    log_info ".NET 10 runtime already present, skipping."
else
    log_info "Downloading .NET 10 runtime..."
    mkdir -p "$DOTNET_DIR"
    curl -sSL https://dot.net/v1/dotnet-install.sh -o /tmp/dotnet-install.sh
    chmod +x /tmp/dotnet-install.sh
    /tmp/dotnet-install.sh --channel 10.0 --runtime dotnet --install-dir "$DOTNET_DIR"

    if [ ! -d "$DOTNET_DIR/shared/Microsoft.NETCore.App" ]; then
        log_error "Failed to download .NET 10 runtime"
        exit 1
    fi
    log_info ".NET 10 runtime installed."
fi

# ─── Download hostfxr headers (if needed) ─────────────────────────────────────
DOTNET_INCLUDE="$DOTNET_DIR/include"
if [ -f "$DOTNET_INCLUDE/hostfxr.h" ] && [ "$(wc -l < "$DOTNET_INCLUDE/hostfxr.h")" -gt 5 ]; then
    log_info "hostfxr headers already present, skipping."
else
    log_info "Downloading hostfxr headers..."
    mkdir -p "$DOTNET_INCLUDE"
    HEADERS_BASE="https://raw.githubusercontent.com/dotnet/runtime/main/src/native/corehost"
    curl -sSL "$HEADERS_BASE/hostfxr.h" -o "$DOTNET_INCLUDE/hostfxr.h"
    curl -sSL "$HEADERS_BASE/coreclr_delegates.h" -o "$DOTNET_INCLUDE/coreclr_delegates.h"
    curl -sSL "$HEADERS_BASE/nethost/nethost.h" -o "$DOTNET_INCLUDE/nethost.h"
    log_info "hostfxr headers downloaded."
fi

# ─── Make premake5 executable ─────────────────────────────────────────────────
chmod +x "$SCRIPT_DIR/vendor/premake/premake5"

# ─── Generate Makefiles ──────────────────────────────────────────────────────
log_info "Generating Makefiles..."
"$SCRIPT_DIR/vendor/premake/premake5" gmake2

# ─── Create VS Code config ───────────────────────────────────────────────────
mkdir -p "$SCRIPT_DIR/.vscode"

cat > "$SCRIPT_DIR/.vscode/tasks.json" << 'EOF'
{
    "version": "2.0.0",
    "tasks": [
        {
            "label": "build-debug",
            "type": "shell",
            "command": "make",
            "args": ["config=debug"],
            "group": {
                "kind": "build",
                "isDefault": true
            }
        },
        {
            "label": "build-release",
            "type": "shell",
            "command": "make",
            "args": ["config=release"]
        }
    ]
}
EOF

cat > "$SCRIPT_DIR/.vscode/launch.json" << 'EOF'
{
    "version": "0.2.0",
    "configurations": [
        {
            "name": "(gdb) Launch Editor",
            "type": "cppdbg",
            "request": "launch",
            "program": "${workspaceFolder}/bin/Debug-linux-x86_64/Editor/Editor",
            "args": [],
            "stopAtEntry": false,
            "cwd": "${workspaceFolder}/Editor",
            "environment": [],
            "externalConsole": false,
            "MIMode": "gdb",
            "setupCommands": [
                {
                    "description": "Enable pretty-printing for gdb",
                    "text": "-enable-pretty-printing",
                    "ignoreFailures": true
                }
            ],
            "preLaunchTask": "build-debug",
            "miDebuggerPath": "/usr/bin/gdb"
        }
    ]
}
EOF

cat > "$SCRIPT_DIR/.vscode/c_cpp_properties.json" << 'EOF'
{
    "configurations": [
        {
            "name": "Linux",
            "includePath": [
                "${workspaceFolder}/Engine/**",
                "${workspaceFolder}/Editor/**"
            ],
            "defines": [
                "DEBUG",
                "PLATFORM_LINUX",
                "GLFW_INCLUDE_NONE"
            ],
            "compilerPath": "/usr/bin/g++",
            "cStandard": "c17",
            "cppStandard": "c++20",
            "intelliSenseMode": "${default}"
        }
    ],
    "version": 4
}
EOF

log_info ""
log_info "========================================="
log_info "  Project setup complete!"
log_info "========================================="
log_info ""
log_info "To build: make config=debug"
log_info "To run:   cd Editor && ../bin/Debug-linux-x86_64/Editor/Editor"
