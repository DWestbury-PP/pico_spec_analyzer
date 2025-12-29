#!/bin/bash
#
# build.sh - Build script for Pico Spectrum Analyzer
#
# Usage:
#   ./scripts/build.sh [clean|debug|release]
#

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Project root directory
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build"

# Parse arguments
BUILD_TYPE="Release"
CLEAN=false

if [ "$1" = "clean" ]; then
    CLEAN=true
elif [ "$1" = "debug" ]; then
    BUILD_TYPE="Debug"
elif [ "$1" = "release" ]; then
    BUILD_TYPE="Release"
elif [ -n "$1" ]; then
    echo -e "${RED}Unknown argument: $1${NC}"
    echo "Usage: $0 [clean|debug|release]"
    exit 1
fi

echo -e "${BLUE}=== Pico Spectrum Analyzer Build Script ===${NC}"
echo ""

# Clean if requested
if [ "$CLEAN" = true ]; then
    echo -e "${YELLOW}Cleaning build directory...${NC}"
    rm -rf "${BUILD_DIR}"
    echo -e "${GREEN}✓ Clean complete${NC}"
    exit 0
fi

# Create build directory
if [ ! -d "${BUILD_DIR}" ]; then
    echo -e "${BLUE}Creating build directory...${NC}"
    mkdir -p "${BUILD_DIR}"
fi

cd "${BUILD_DIR}"

# Run CMake
echo -e "${BLUE}Running CMake (${BUILD_TYPE})...${NC}"
cmake .. \
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
    -DPICO_BOARD=pico_w

echo ""
echo -e "${BLUE}Compiling project...${NC}"

# Build with all available cores
NPROC=$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)
make -j"${NPROC}"

echo ""
echo -e "${GREEN}=== Build Complete ===${NC}"
echo ""
echo "Output files:"
ls -lh "${BUILD_DIR}/pico_spec_analyzer."* 2>/dev/null || true
echo ""
echo -e "${GREEN}✓ Success!${NC}"
echo ""
echo "To flash the Pico:"
echo "  1. Hold BOOTSEL button on Pico"
echo "  2. Connect USB cable"
echo "  3. Release BOOTSEL (Pico appears as USB drive)"
echo "  4. Run: ./scripts/flash.sh"
echo "     Or: cp build/pico_spec_analyzer.uf2 /Volumes/RPI-RP2/"

