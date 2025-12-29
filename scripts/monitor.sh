#!/bin/bash
#
# monitor.sh - Serial monitor for Pico Spectrum Analyzer
#
# This script connects to the Pico's serial port to view debug output
#

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

BAUD_RATE=115200

echo -e "${BLUE}=== Pico Serial Monitor ===${NC}"
echo ""

# Find Pico serial port
# macOS: /dev/tty.usbmodem*
# Linux: /dev/ttyACM*

SERIAL_PORT=""

if [ "$(uname)" = "Darwin" ]; then
    # macOS
    SERIAL_PORT=$(ls /dev/tty.usbmodem* 2>/dev/null | head -n 1)
elif [ "$(uname)" = "Linux" ]; then
    # Linux
    SERIAL_PORT=$(ls /dev/ttyACM* 2>/dev/null | head -n 1)
fi

if [ -z "$SERIAL_PORT" ]; then
    echo -e "${RED}Error: Could not find Pico serial port${NC}"
    echo ""
    echo "Make sure:"
    echo "  1. Pico is connected via USB"
    echo "  2. Firmware is running (not in BOOTSEL mode)"
    echo "  3. USB serial is enabled in firmware"
    echo ""
    
    if [ "$(uname)" = "Darwin" ]; then
        echo "Looking for: /dev/tty.usbmodem*"
        echo "Available ports:"
        ls /dev/tty.* 2>/dev/null | grep -i usb || echo "  (none found)"
    else
        echo "Looking for: /dev/ttyACM*"
        echo "Available ports:"
        ls /dev/ttyACM* 2>/dev/null || echo "  (none found)"
    fi
    
    exit 1
fi

echo -e "${GREEN}Found Pico at:${NC} ${SERIAL_PORT}"
echo -e "${GREEN}Baud rate:${NC} ${BAUD_RATE}"
echo ""
echo -e "${YELLOW}Press Ctrl+C to exit${NC}"
echo -e "${BLUE}───────────────────────────────────────────────${NC}"
echo ""

# Check if screen is available
if command -v screen &> /dev/null; then
    # Use screen
    screen "${SERIAL_PORT}" ${BAUD_RATE}
elif command -v minicom &> /dev/null; then
    # Use minicom
    minicom -D "${SERIAL_PORT}" -b ${BAUD_RATE}
elif command -v picocom &> /dev/null; then
    # Use picocom
    picocom -b ${BAUD_RATE} "${SERIAL_PORT}"
else
    echo -e "${RED}Error: No serial terminal program found${NC}"
    echo ""
    echo "Please install one of the following:"
    echo "  macOS: screen (built-in) or brew install minicom"
    echo "  Linux: sudo apt install screen (or minicom or picocom)"
    echo ""
    echo "Or manually connect with:"
    echo "  screen ${SERIAL_PORT} ${BAUD_RATE}"
    exit 1
fi

