#!/bin/bash
#
# flash.sh - Flash script for Pico Spectrum Analyzer
#
# This script copies the compiled .uf2 file to the Pico when it's in BOOTSEL mode
#

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# Project root
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
UF2_FILE="${PROJECT_ROOT}/build/pico_spec_analyzer.uf2"

# Possible mount points for Pico in BOOTSEL mode
MOUNT_POINTS=(
    "/Volumes/RPI-RP2"      # macOS
    "/media/${USER}/RPI-RP2" # Linux
    "/mnt/RPI-RP2"           # Linux alternative
)

echo -e "${BLUE}=== Pico Flash Script ===${NC}"
echo ""

# Check if UF2 file exists
if [ ! -f "${UF2_FILE}" ]; then
    echo -e "${RED}Error: UF2 file not found: ${UF2_FILE}${NC}"
    echo ""
    echo "Please build the project first:"
    echo "  docker-compose run --rm build"
    echo "  or: ./scripts/build.sh"
    exit 1
fi

echo -e "${GREEN}Found UF2 file:${NC} ${UF2_FILE}"
echo -e "Size: $(ls -lh "${UF2_FILE}" | awk '{print $5}')"
echo ""

# Find Pico mount point
PICO_MOUNT=""
for mount in "${MOUNT_POINTS[@]}"; do
    if [ -d "$mount" ]; then
        PICO_MOUNT="$mount"
        break
    fi
done

if [ -z "$PICO_MOUNT" ]; then
    echo -e "${YELLOW}Pico not found in BOOTSEL mode${NC}"
    echo ""
    echo "To enter BOOTSEL mode:"
    echo "  1. Hold the BOOTSEL button on the Pico"
    echo "  2. Connect the USB cable to your computer"
    echo "  3. Release the BOOTSEL button"
    echo ""
    echo "The Pico should appear as a USB drive named 'RPI-RP2'"
    echo ""
    echo "Then run this script again."
    exit 1
fi

echo -e "${GREEN}Found Pico at:${NC} ${PICO_MOUNT}"
echo ""
echo -e "${BLUE}Copying UF2 file to Pico...${NC}"

# Copy file
cp "${UF2_FILE}" "${PICO_MOUNT}/"

echo -e "${GREEN}✓ Flash complete!${NC}"
echo ""
echo "The Pico will automatically reboot and start running the new firmware."
echo ""
echo "To monitor serial output, run:"
echo "  ./scripts/monitor.sh"

