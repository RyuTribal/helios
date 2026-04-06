#!/bin/bash
set -e
SHADER_DIR="Editor/Resources/Shaders"
for f in "$SHADER_DIR"/*.vert "$SHADER_DIR"/*.frag "$SHADER_DIR"/*.comp "$SHADER_DIR"/*.geo; do
    [ -f "$f" ] || continue
    echo "Compiling: $(basename $f)"
    case "$f" in
        *.geo) glslc -fshader-stage=geometry "$f" -o "$f.spv" ;;
        *)     glslc "$f" -o "$f.spv" ;;
    esac
done
echo "All shaders compiled successfully."
