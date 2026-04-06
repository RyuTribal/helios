#!/bin/bash
vendor/premake/premake5 gmake2

PROJECT_ROOT="$(dirname "$0")"

# Create .vscode directory if it doesn't exist
mkdir -p "$PROJECT_ROOT/.vscode"

# Create tasks.json for building the project
cat > "$PROJECT_ROOT/.vscode/tasks.json" << 'EOF'
{
    "version": "2.0.0",
    "tasks": [
        {
            "label": "build-debug",
            "type": "shell",
            "command": "make",
            "args": ["config=debug_linux-x86_64"],
            "group": {
                "kind": "build",
                "isDefault": true
            }
        },
        {
            "label": "build-release",
            "type": "shell",
            "command": "make",
            "args": ["config=release_linux-x86_64"]
        }
    ]
}
EOF

# Create launch.json for debugging
cat > "$PROJECT_ROOT/.vscode/launch.json" << 'EOF'
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

cat > "$PROJECT_ROOT/.vscode/c_cpp_properties.json" << 'EOF'
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

echo "VS Code configuration files have been created/updated."
echo ""
echo "To build: make config=debug_linux-x86_64"
