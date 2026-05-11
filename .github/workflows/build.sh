#!/bin/bash
# Native Addon Build Script for Linux/macOS
# Run this from the root of your addon folder (where package.json is)
#
# Usage: ./build.sh [binary_name] [config]
#   binary_name - Optional. Defaults to folder name if not specified.
#   config      - Optional. "Debug", "Release", or "Both" (default: Both)
#
# Environment:
#   POLYPHASE_PATH - Path to Polyphase engine installation (required for engine headers)
#
# Requirements:
#   - g++ or clang++ installed
#   - Standard C++ development libraries
#
# Output:
#   build/Linux/x64/Release/lib<binary_name>.so
#   build/Linux/x64/Debug/lib<binary_name>.so

# Get addon folder name as default binary name
FOLDER_NAME=$(basename "$(pwd)")

# Use argument or default to folder name
ADDON_NAME="${1:-$FOLDER_NAME}"

# Config: Debug, Release, or Both (default)
BUILD_CONFIG="${2:-Both}"

echo ""
echo "========================================"
echo " Building Native Addon: $ADDON_NAME"
echo " Configuration: $BUILD_CONFIG"
echo "========================================"
echo ""

# Determine addon root (script may be in .github/workflows/ or addon root)
SCRIPT_SOURCE="${BASH_SOURCE:-$0}"
SCRIPT_DIR="$(cd "$(dirname "$SCRIPT_SOURCE")" && pwd)"
ADDON_ROOT="."
if [ -d "$SCRIPT_DIR/../../Source" ]; then
    ADDON_ROOT="$SCRIPT_DIR/../.."
fi

# Check for Source directory
if [ ! -d "$ADDON_ROOT/Source" ]; then
    echo "ERROR: Source directory not found!"
    echo "Make sure you're running this from the addon root folder or .github/workflows/."
    exit 1
fi

# Set build output directory
BUILD_DIR="$ADDON_ROOT/build"

# Check for compiler
if command -v g++ &> /dev/null; then
    CXX="g++"
elif command -v clang++ &> /dev/null; then
    CXX="clang++"
else
    echo "ERROR: No C++ compiler found!"
    echo "Please install g++ or clang++."
    echo ""
    echo "On Ubuntu/Debian: sudo apt install g++"
    echo "On Fedora:        sudo dnf install gcc-c++"
    echo "On Arch:          sudo pacman -S gcc"
    exit 1
fi

echo "Using compiler: $CXX"
echo ""

# Gather all .cpp files
SOURCES=$(find "$ADDON_ROOT/Source" -name "*.cpp" -type f)

if [ -z "$SOURCES" ]; then
    echo "ERROR: No .cpp files found in Source directory!"
    exit 1
fi

echo "Found source files:"
for f in $SOURCES; do
    echo "  $(basename $f)"
done
echo ""

# Build include paths
INCLUDE_FLAGS="-I$ADDON_ROOT/Source"

if [ -n "$POLYPHASE_PATH" ]; then
    if [ ! -d "$POLYPHASE_PATH/Engine/Source" ]; then
        echo "ERROR: POLYPHASE_PATH does not point to a valid engine root."
        echo "  POLYPHASE_PATH = $POLYPHASE_PATH"
        echo "  Expected to find: $POLYPHASE_PATH/Engine/Source"
        echo ""
        echo "Set POLYPHASE_PATH to the directory containing Engine/ and External/."
        exit 1
    fi
    echo "Using Polyphase engine at: $POLYPHASE_PATH"
    INCLUDE_FLAGS="$INCLUDE_FLAGS -I$POLYPHASE_PATH/Engine/Source"
    INCLUDE_FLAGS="$INCLUDE_FLAGS -I$POLYPHASE_PATH/Engine/Source/Engine"
    INCLUDE_FLAGS="$INCLUDE_FLAGS -I$POLYPHASE_PATH/Engine/Source/Editor"
    INCLUDE_FLAGS="$INCLUDE_FLAGS -I$POLYPHASE_PATH/Engine/Source/Plugins"
    INCLUDE_FLAGS="$INCLUDE_FLAGS -I$POLYPHASE_PATH/External"
    INCLUDE_FLAGS="$INCLUDE_FLAGS -I$POLYPHASE_PATH/External/Assimp"
    INCLUDE_FLAGS="$INCLUDE_FLAGS -I$POLYPHASE_PATH/External/Bullet"
    INCLUDE_FLAGS="$INCLUDE_FLAGS -I$POLYPHASE_PATH/External/Lua"
    INCLUDE_FLAGS="$INCLUDE_FLAGS -I$POLYPHASE_PATH/External/glm"
    INCLUDE_FLAGS="$INCLUDE_FLAGS -I$POLYPHASE_PATH/External/Imgui"
    INCLUDE_FLAGS="$INCLUDE_FLAGS -I$POLYPHASE_PATH/External/ImGuizmo"
    INCLUDE_FLAGS="$INCLUDE_FLAGS -I$POLYPHASE_PATH/External/Vorbis"

    # Vulkan headers — prefer $VULKAN_SDK (LunarG SDK / Docker image), fall back
    # to system headers (libvulkan-dev) which gcc finds on its default path.
    if [ -n "$VULKAN_SDK" ] && [ -d "$VULKAN_SDK/include" ]; then
        echo "Using Vulkan SDK at: $VULKAN_SDK"
        INCLUDE_FLAGS="$INCLUDE_FLAGS -I$VULKAN_SDK/include"
    elif [ ! -f /usr/include/vulkan/vulkan.h ]; then
        echo "WARNING: Vulkan headers not found."
        echo "  Set VULKAN_SDK to a LunarG SDK install, or 'sudo apt install libvulkan-dev'."
    fi
    echo ""

    # Add common engine defines
    ENGINE_DEFINES="-DEDITOR=1 -DLUA_ENABLED=1 -DGLM_FORCE_RADIANS -DAPI_VULKAN=1"
else
    echo "Note: POLYPHASE_PATH not set. Only addon Source/ will be included."
    echo "      Set POLYPHASE_PATH for addons that use engine headers."
    echo ""
    ENGINE_DEFINES=""
fi

BUILD_FAILED=0

# Function to generate checksum
generate_checksum() {
    local file="$1"
    local checksum_file="$2"
    if command -v sha256sum &> /dev/null; then
        sha256sum "$file" > "$checksum_file"
    elif command -v shasum &> /dev/null; then
        shasum -a 256 "$file" > "$checksum_file"
    fi
}

# Build Release if requested
if [ "$BUILD_CONFIG" = "Release" ] || [ "$BUILD_CONFIG" = "Both" ]; then
    echo "----------------------------------------"
    echo "Building Release configuration..."
    echo "----------------------------------------"
    echo ""

    mkdir -p "$BUILD_DIR/Linux/x64/Release"

    if $CXX -shared -fPIC -O2 -std=c++17 \
        $INCLUDE_FLAGS \
        $ENGINE_DEFINES \
        -DOCTAVE_PLUGIN_EXPORT \
        -DNDEBUG \
        -DPLATFORM_LINUX=1 \
        -o "$BUILD_DIR/Linux/x64/Release/lib${ADDON_NAME}.so" \
        $SOURCES; then
        echo "Release build succeeded: $BUILD_DIR/Linux/x64/Release/lib${ADDON_NAME}.so"
        generate_checksum "$BUILD_DIR/Linux/x64/Release/lib${ADDON_NAME}.so" "$BUILD_DIR/Linux/x64/Release/${ADDON_NAME}-Linux-x64-Release.sha256"
    else
        echo "Release build FAILED!"
        BUILD_FAILED=1
    fi
    echo ""
fi

# Build Debug if requested
if [ "$BUILD_CONFIG" = "Debug" ] || [ "$BUILD_CONFIG" = "Both" ]; then
    echo "----------------------------------------"
    echo "Building Debug configuration..."
    echo "----------------------------------------"
    echo ""

    mkdir -p "$BUILD_DIR/Linux/x64/Debug"

    if $CXX -shared -fPIC -O0 -g -std=c++17 \
        $INCLUDE_FLAGS \
        $ENGINE_DEFINES \
        -DOCTAVE_PLUGIN_EXPORT \
        -D_DEBUG \
        -DPLATFORM_LINUX=1 \
        -o "$BUILD_DIR/Linux/x64/Debug/lib${ADDON_NAME}.so" \
        $SOURCES; then
        echo "Debug build succeeded: $BUILD_DIR/Linux/x64/Debug/lib${ADDON_NAME}.so"
        generate_checksum "$BUILD_DIR/Linux/x64/Debug/lib${ADDON_NAME}.so" "$BUILD_DIR/Linux/x64/Debug/${ADDON_NAME}-Linux-x64-Debug.sha256"
    else
        echo "Debug build FAILED!"
        BUILD_FAILED=1
    fi
    echo ""
fi

echo ""
if [ $BUILD_FAILED -eq 1 ]; then
    echo "========================================"
    echo " BUILD COMPLETED WITH ERRORS"
    echo "========================================"
else
    echo "========================================"
    echo " Build Succeeded!"
    echo "========================================"

    # Auto-update package.json with binary descriptors
    if [ -f "$ADDON_ROOT/package.json" ]; then
        echo ""
        echo "Updating package.json with binary descriptors..."

        if command -v jq &> /dev/null; then
            # Use jq for proper JSON manipulation
            TEMP_FILE=$(mktemp)

            # Start with existing binaries or empty array
            cp "$ADDON_ROOT/package.json" "$TEMP_FILE"

            # Add Release binary if built
            if [ -f "$BUILD_DIR/Linux/x64/Release/lib${ADDON_NAME}.so" ]; then
                jq --arg name "lib${ADDON_NAME}-Linux-x64-Release.so" \
                   'if .binaries == null then .binaries = [] else . end |
                    if (.binaries | map(select(.platform == "Linux" and .arch == "x64" and .config == "Release")) | length) == 0
                    then .binaries += [{"platform": "Linux", "arch": "x64", "config": "Release", "type": "releaseAsset", "value": $name}]
                    else . end' "$TEMP_FILE" > package.json.tmp && mv package.json.tmp "$TEMP_FILE"
            fi

            # Add Debug binary if built
            if [ -f "$BUILD_DIR/Linux/x64/Debug/lib${ADDON_NAME}.so" ]; then
                jq --arg name "lib${ADDON_NAME}-Linux-x64-Debug.so" \
                   'if .binaries == null then .binaries = [] else . end |
                    if (.binaries | map(select(.platform == "Linux" and .arch == "x64" and .config == "Debug")) | length) == 0
                    then .binaries += [{"platform": "Linux", "arch": "x64", "config": "Debug", "type": "releaseAsset", "value": $name}]
                    else . end' "$TEMP_FILE" > package.json.tmp && mv package.json.tmp "$ADDON_ROOT/package.json"
            else
                mv "$TEMP_FILE" "$ADDON_ROOT/package.json"
            fi

            echo "  Added Linux binary descriptors to package.json"
        else
            echo "  Note: Install jq for automatic package.json updates"
            echo "  Manual update needed - add these to package.json binaries array:"
            [ -f "$BUILD_DIR/Linux/x64/Release/lib${ADDON_NAME}.so" ] && \
                echo "    {\"platform\": \"Linux\", \"arch\": \"x64\", \"config\": \"Release\", \"type\": \"releaseAsset\", \"value\": \"lib${ADDON_NAME}-Linux-x64-Release.so\"}"
            [ -f "$BUILD_DIR/Linux/x64/Debug/lib${ADDON_NAME}.so" ] && \
                echo "    {\"platform\": \"Linux\", \"arch\": \"x64\", \"config\": \"Debug\", \"type\": \"releaseAsset\", \"value\": \"lib${ADDON_NAME}-Linux-x64-Debug.so\"}"
        fi
    fi
fi
echo ""
echo "Output directory: $BUILD_DIR/Linux/x64/"
if [ "$BUILD_CONFIG" = "Both" ]; then
    echo "  Release/lib${ADDON_NAME}.so"
    echo "  Debug/lib${ADDON_NAME}.so"
else
    echo "  ${BUILD_CONFIG}/lib${ADDON_NAME}.so"
fi
echo ""
echo "----------------------------------------"
echo "To test in Polyphase:"
echo "  1. Copy the appropriate .so to your project's"
echo "     Intermediate/Plugins/${ADDON_NAME}/Synced/ folder"
echo "  2. Set the addon to Binary mode in the Addons window"
echo "  3. Click Reload to load the binary"
echo "----------------------------------------"
echo ""

exit $BUILD_FAILED
