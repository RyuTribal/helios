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
for cmd in cmake make gcc g++ pkg-config git autoconf automake libtool; do
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

# ─── Build/Install Mono ──────────────────────────────────────────────────────
MONO_LIB_DIR="$VENDOR_DIR/mono/lib/linux"
if [ -f "$MONO_LIB_DIR/libmonosgen-2.0.a" ] || [ -f "$MONO_LIB_DIR/libmonosgen-2.0.so" ]; then
    log_info "Mono Linux library already exists, skipping."
else
    log_info "Setting up Mono for Linux..."
    mkdir -p "$MONO_LIB_DIR"

    # Try to find system-installed Mono first
    MONO_SYSTEM_LIB=""
    for search_path in /usr/lib /usr/lib64 /usr/local/lib /usr/lib/x86_64-linux-gnu; do
        if [ -f "$search_path/libmonosgen-2.0.a" ]; then
            MONO_SYSTEM_LIB="$search_path/libmonosgen-2.0.a"
            break
        elif [ -f "$search_path/libmonosgen-2.0.so" ]; then
            MONO_SYSTEM_LIB="$search_path/libmonosgen-2.0.so"
            break
        fi
    done

    if [ -n "$MONO_SYSTEM_LIB" ]; then
        log_info "Found system Mono at: $MONO_SYSTEM_LIB"
        cp "$MONO_SYSTEM_LIB" "$MONO_LIB_DIR/"

        # Also copy the shared lib if we found the static one, or vice versa
        MONO_DIR="$(dirname "$MONO_SYSTEM_LIB")"
        for f in "$MONO_DIR"/libmonosgen-2.0.*; do
            [ -f "$f" ] && cp "$f" "$MONO_LIB_DIR/" 2>/dev/null || true
        done

        log_info "Mono libraries copied to $MONO_LIB_DIR"
    else
        log_warn "System Mono not found. Attempting to build from source..."
        log_warn "This may take a long time (30+ minutes)."

        MONO_BUILD="$BUILD_DIR/mono"
        rm -rf "$MONO_BUILD"

        git clone --depth 1 --branch mono-6.12.0.206 https://github.com/mono/mono.git "$MONO_BUILD"

        cd "$MONO_BUILD"
        ./autogen.sh --prefix="$MONO_BUILD/install" \
            --disable-boehm \
            --enable-static \
            --with-sgen=yes

        make -j "$(nproc)"
        make install

        # Copy the built libraries
        cp "$MONO_BUILD/install/lib"/libmonosgen-2.0.* "$MONO_LIB_DIR/" 2>/dev/null || true
        cp "$MONO_BUILD/mono/mini/.libs"/libmonosgen-2.0.* "$MONO_LIB_DIR/" 2>/dev/null || true

        cd "$SCRIPT_DIR"
        log_info "Mono built and installed to $MONO_LIB_DIR"
    fi
fi

# ─── Verify mono include headers ─────────────────────────────────────────────
MONO_INCLUDE="$VENDOR_DIR/mono/include/mono"
if [ ! -d "$MONO_INCLUDE/jit" ]; then
    log_warn "Mono include headers may be incomplete."
    # Try to copy from system
    for search_path in /usr/include/mono-2.0 /usr/local/include/mono-2.0; do
        if [ -d "$search_path/mono" ]; then
            log_info "Copying Mono headers from $search_path..."
            cp -r "$search_path/mono/"* "$MONO_INCLUDE/"
            break
        fi
    done
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
log_info "  1. ./generate_linux_projects.sh"
log_info "  2. make config=debug_linux-x86_64"
log_info ""

# Optionally clean build dir
read -p "Clean up temporary build files in $BUILD_DIR? [y/N] " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    rm -rf "$BUILD_DIR"
    log_info "Cleaned up build directory."
fi
