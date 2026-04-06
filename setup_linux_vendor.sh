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
for cmd in cmake make gcc g++ pkg-config git curl; do
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
    exit 1
fi

log_info "All prerequisites found."

mkdir -p "$BUILD_DIR"

# ─── Build Assimp ─────────────────────────────────────────────────────────────
ASSIMP_LIB_DIR="$VENDOR_DIR/assimp/lib/linux-x64"
if [ -f "$ASSIMP_LIB_DIR/libassimp.a" ]; then
    log_info "Assimp Linux library already exists, skipping build."
else
    log_info "Building Assimp from source..."
    ASSIMP_BUILD="$BUILD_DIR/assimp"
    rm -rf "$ASSIMP_BUILD"

    git clone --depth 1 --branch v5.4.3 https://github.com/assimp/assimp.git "$ASSIMP_BUILD"

    mkdir -p "$ASSIMP_BUILD/build"
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

    # Also copy the zlib static lib if assimp built it
    if [ -f "$ASSIMP_BUILD/build/contrib/zlib/libzlibstatic.a" ]; then
        cp "$ASSIMP_BUILD/build/contrib/zlib/libzlibstatic.a" "$ASSIMP_LIB_DIR/"
    fi

    log_info "Assimp built and installed to $ASSIMP_LIB_DIR"
fi

# ─── Download .NET 10 Runtime ─────────────────────────────────────────────────
DOTNET_DIR="$VENDOR_DIR/dotnet"
if [ -d "$DOTNET_DIR/shared/Microsoft.NETCore.App" ]; then
    log_info ".NET 10 runtime already exists, skipping download."
else
    log_info "Downloading .NET 10 runtime..."
    mkdir -p "$DOTNET_DIR"

    # Use the official dotnet-install script
    curl -sSL https://dot.net/v1/dotnet-install.sh -o /tmp/dotnet-install.sh
    chmod +x /tmp/dotnet-install.sh
    /tmp/dotnet-install.sh --channel 10.0 --runtime dotnet --install-dir "$DOTNET_DIR"

    if [ ! -d "$DOTNET_DIR/shared/Microsoft.NETCore.App" ]; then
        log_error "Failed to download .NET 10 runtime"
        exit 1
    fi

    log_info ".NET 10 runtime installed to $DOTNET_DIR"
fi

# ─── Download hostfxr headers ─────────────────────────────────────────────────
DOTNET_INCLUDE="$DOTNET_DIR/include"
if [ -f "$DOTNET_INCLUDE/hostfxr.h" ] && [ "$(wc -l < "$DOTNET_INCLUDE/hostfxr.h")" -gt 5 ]; then
    log_info "hostfxr headers already present, skipping download."
else
    log_info "Downloading hostfxr headers..."
    mkdir -p "$DOTNET_INCLUDE"
    HEADERS_BASE="https://raw.githubusercontent.com/dotnet/runtime/main/src/native/corehost"
    curl -sSL "$HEADERS_BASE/hostfxr.h" -o "$DOTNET_INCLUDE/hostfxr.h"
    curl -sSL "$HEADERS_BASE/coreclr_delegates.h" -o "$DOTNET_INCLUDE/coreclr_delegates.h"
    curl -sSL "$HEADERS_BASE/nethost/nethost.h" -o "$DOTNET_INCLUDE/nethost.h"
    log_info "hostfxr headers downloaded to $DOTNET_INCLUDE"
fi

# ─── Make premake5 executable ─────────────────────────────────────────────────
chmod +x "$SCRIPT_DIR/vendor/premake/premake5"
log_info "Made premake5 executable."

# ─── Cleanup ──────────────────────────────────────────────────────────────────
log_info ""
log_info "========================================="
log_info "  Vendor setup complete!"
log_info "========================================="
log_info ""
log_info "Next steps:"
log_info "  1. Install .NET 10 SDK: https://dot.net/download"
log_info "  2. dotnet build ScriptCore/ScriptCore.csproj"
log_info "  3. ./generate_linux_projects.sh"
log_info "  4. make config=debug"
log_info ""

# Optionally clean build dir
read -p "Clean up temporary build files in $BUILD_DIR? [y/N] " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    rm -rf "$BUILD_DIR"
    log_info "Cleaned up build directory."
fi
