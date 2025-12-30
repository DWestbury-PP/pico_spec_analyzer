# Quick Start Guide

Get up and running with Pico Spectrum Analyzer in minutes!

## Prerequisites

- ✅ Docker Desktop for Mac (running)
- ✅ Raspberry Pi Pico W
- ✅ USB cable

## Step 1: Build the Docker Environment

From the project directory:

```bash
cd "/Users/dwestbury/Documents/Source Code/RPI_Pico/pico_spec_analyzer"
docker-compose build
```

This downloads and configures the development environment (first time only, ~5 minutes).

## Step 2: Compile the Firmware

### Option A: Using Docker Compose (Recommended)

```bash
docker-compose run --rm build
```

### Option B: Using the Build Script

```bash
./scripts/docker-build.sh
```

### Option C: Interactive Development

```bash
docker-compose run --rm dev bash

# Inside the container:
mkdir -p build && cd build
cmake ..
make -j4
exit
```

## Step 3: Flash to Pico

1. **Enter BOOTSEL Mode:**
   - Hold the **BOOTSEL** button on your Pico W
   - Connect USB cable to your Mac
   - Release BOOTSEL button
   - Pico appears as USB drive named **RPI-RP2**

2. **Flash the Firmware:**

   ```bash
   ./scripts/flash.sh
   ```

   Or manually:
   ```bash
   cp build/pico_spec_analyzer.uf2 /Volumes/RPI-RP2/
   ```

3. **Done!** The Pico automatically reboots and runs your code.

## Step 4: Monitor Serial Output

```bash
./scripts/monitor.sh
```

Press `Ctrl+C` to exit.

## Development Workflow

### Make Changes and Rebuild

```bash
# Edit source files in src/ or include/
# Then rebuild:
docker-compose run --rm build

# Flash:
./scripts/flash.sh

# Monitor:
./scripts/monitor.sh
```

### Clean Build

```bash
docker-compose run --rm clean
```

Or:

```bash
./scripts/build.sh clean
```

## Current Project Status

🚧 **Phase 1: Initial Setup** ✅ Complete

- [x] Docker development environment
- [x] CMake build system with PIO support
- [x] Project structure
- [x] Basic main.c with dual-core setup
- [x] Hardware pin configuration
- [x] Build and flash scripts

🚧 **Phase 2: Audio Input** (Next)

- [ ] PIO-based ADC sampler
- [ ] Audio input switching (mic/jack)
- [ ] Ring buffer for audio samples

🚧 **Phase 3: FFT Processing**

- [ ] Integrate CMSIS-DSP library
- [ ] Implement FFT processor
- [ ] Frequency band extraction

🚧 **Phase 4: Display Driver**

- [ ] ILI9341 SPI driver
- [ ] DMA transfers
- [ ] Basic graphics primitives

🚧 **Phase 5: Visualizations**

- [ ] Bar graph theme
- [ ] Waterfall theme
- [ ] Radial theme

🚧 **Phase 6: Touch Input**

- [ ] XPT2046 driver
- [ ] Touch gesture handling
- [ ] UI for settings

## Hardware Setup

The current code is configured for the following connections:

### Display (ILI9341)
- GP16: MISO (not used)
- GP17: CS (Chip Select)
- GP18: SCK (Clock)
- GP19: MOSI (Data)
- GP20: DC (Data/Command)
- GP21: RST (Reset)
- GP22: BL (Backlight)

### Touch (XPT2046)
- GP12: MISO
- GP13: CS
- GP14: SCK
- GP15: MOSI
- GP23: IRQ

### Audio
- GP26 (ADC0): Microphone
- GP27 (ADC1): Audio jack
- GP10: Input selector

See `include/config.h` for complete pin configuration.

## Troubleshooting

### Docker Issues

**Problem:** "Cannot connect to Docker daemon"
```bash
# Make sure Docker Desktop is running
open -a Docker
```

**Problem:** Build is slow on first run
- This is normal - Docker is downloading ARM toolchain and Pico SDK
- Subsequent builds are much faster

### Build Issues

**Problem:** "PICO_SDK_PATH not found"
- This is handled automatically in Docker
- SDK is at `/opt/pico-sdk` inside container

**Problem:** CMake errors
```bash
# Clean and rebuild
docker-compose run --rm clean
docker-compose run --rm build
```

### Flashing Issues

**Problem:** "Pico not found in BOOTSEL mode"
- Make sure you're holding BOOTSEL when connecting USB
- Try a different USB cable (some are power-only)
- Check `/Volumes/` for RPI-RP2 drive

**Problem:** Flash succeeds but nothing happens
- Check USB serial output: `./scripts/monitor.sh`
- LED should blink during initialization
- Current firmware only blinks LED (display code pending)

## Next Steps

1. **Test the basic firmware:**
   - Flash the current code
   - Verify LED blinks
   - Check serial output shows initialization messages

2. **Start implementing features:**
   - See `src/` directory for module stubs
   - Each module has TODO comments
   - Build incrementally and test often

3. **Review documentation:**
   - Main README: `README.md`
   - Configuration: `include/config.h`
   - Architecture diagram in README

## Need Help?

- Check `README.md` for detailed architecture
- Review datasheets in `datasheets-and-manuals/`
- Examine `include/config.h` for all settings
- Use `DEBUG_PRINTF()` for debugging output

## Useful Commands Reference

```bash
# Build
docker-compose run --rm build

# Clean
docker-compose run --rm clean

# Interactive shell
docker-compose run --rm dev bash

# Flash
./scripts/flash.sh

# Monitor serial
./scripts/monitor.sh

# View project structure
tree -L 2 -I 'build|asset_images|datasheets-and-manuals'
```

Happy coding! 🎵🎨🎧

