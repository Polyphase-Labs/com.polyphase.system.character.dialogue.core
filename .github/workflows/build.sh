#!/bin/bash
# Native Addon Build Script for Linux/macOS
# Run this from the root of your addon folder (where package.json is)
#
# Usage: ./build.sh [binary_name]
#   binary_name - Optional. Defaults to folder name if not specified.
#
# Requirements:
#   - g++ or clang++ installed
#   - Standard C++ development libraries
#
# Output:
#   build/Linux/x64/lib<binary_name>.so
#   build/Linux/x64/<binary_name>-Linux-x64.sha256

set -e

# Get addon folder name as default binary name
FOLDER_NAME=$(basename "$(pwd)")

# Use argument or default to folder name
ADDON_NAME="${1:-$FOLDER_NAME}"

echo ""
echo "========================================"
echo " Building Native Addon: $ADDON_NAME"
echo "========================================"
echo ""

# Check for Source directory
if [ ! -d "Source" ]; then
    echo "ERROR: Source directory not found!"
    echo "Make sure you're running this from the addon root folder."
    exit 1
fi

# Create build directory
mkdir -p build/Linux/x64

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
SOURCES=$(find Source -name "*.cpp" -type f)

if [ -z "$SOURCES" ]; then
    echo "ERROR: No .cpp files found in Source directory!"
    exit 1
fi

echo "Found source files:"
for f in $SOURCES; do
    echo "  $(basename $f)"
done
echo ""

# Build Release version
echo "Building Release configuration..."
echo ""

$CXX -shared -fPIC -O2 -std=c++17 \
    -ISource \
    -DOCTAVE_PLUGIN_EXPORT \
    -DNDEBUG \
    -DPLATFORM_LINUX=1 \
    -o "build/Linux/x64/lib${ADDON_NAME}.so" \
    $SOURCES

echo ""
echo "========================================"
echo " Build Succeeded!"
echo "========================================"
echo ""
echo "Output: build/Linux/x64/lib${ADDON_NAME}.so"
echo ""

# Generate checksum
echo "Generating SHA256 checksum..."
if command -v sha256sum &> /dev/null; then
    sha256sum "build/Linux/x64/lib${ADDON_NAME}.so" > "build/Linux/x64/${ADDON_NAME}-Linux-x64.sha256"
    echo "Checksum: build/Linux/x64/${ADDON_NAME}-Linux-x64.sha256"
    cat "build/Linux/x64/${ADDON_NAME}-Linux-x64.sha256"
elif command -v shasum &> /dev/null; then
    shasum -a 256 "build/Linux/x64/lib${ADDON_NAME}.so" > "build/Linux/x64/${ADDON_NAME}-Linux-x64.sha256"
    echo "Checksum: build/Linux/x64/${ADDON_NAME}-Linux-x64.sha256"
    cat "build/Linux/x64/${ADDON_NAME}-Linux-x64.sha256"
fi

echo ""
echo "----------------------------------------"
echo "To test in Polyphase:"
echo "  1. Copy build/Linux/x64/lib${ADDON_NAME}.so to your project's"
echo "     Intermediate/Plugins/${ADDON_NAME}/Synced/ folder"
echo "  2. Set the addon to Binary mode in the Addons window"
echo "  3. Click Reload to load the binary"
echo "----------------------------------------"
echo ""
