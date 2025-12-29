# Raspberry Pi Pico Spectrum Analyzer

A real-time audio spectrum analyzer for the Raspberry Pi Pico W, featuring FFT-based frequency analysis with multiple visualization themes on an SPI TFT display.

## 📋 Project Overview

This project creates a minimalist yet powerful spectrum analyzer that processes audio input in real-time, breaks it down into configurable frequency bands using Fast Fourier Transform (FFT), and displays the results with beautiful visualizations on a 2.8" ILI9341 TFT display.

### Key Features

- **Dual Audio Input Support**
  - MAX4466 electret microphone with adjustable gain
  - 3.5mm audio jack input
  - Analog multiplexer for seamless switching
  
- **Real-Time FFT Processing**
  - ARM CMSIS-DSP optimized FFT library
  - Configurable band count: 4, 8, 16, or 32 frequency bands
  - Sample rates: 8kHz, 16kHz, 22.05kHz
  - Sub-millisecond latency for responsive visuals

- **Multiple Visualization Themes**
  - Classic vertical bar graph
  - Waterfall spectrogram
  - Radial/circular spectrum
  - VU meter style
  - Mirror mode (stereo effect)
  
- **Touch Control Interface**
  - XPT2046 resistive touch controller
  - Swipe to change visualization themes
  - Tap to adjust frequency band count
  - Settings menu for gain and sensitivity
  - Auto-gain toggle

- **PIO-Accelerated Audio Sampling**
  - Programmable I/O for precise ADC timing
  - Reduces CPU load on main cores
  - Consistent sample intervals for accurate FFT

## 🏗️ Architecture

### Dual-Core Processing Strategy

```
┌─────────────────────────────────────────────────────────────┐
│                    Raspberry Pi Pico W                      │
│                     (RP2040 Dual Core)                      │
├──────────────────────────────┬──────────────────────────────┤
│         Core 0               │         Core 1               │
│    (Audio Processing)        │    (Display & UI)            │
├──────────────────────────────┼──────────────────────────────┤
│ • PIO-based ADC sampling     │ • SPI display rendering      │
│ • FFT computation (CMSIS)    │ • DMA transfers              │
│ • Frequency band extraction  │ • Touch input handling       │
│ • Peak detection             │ • UI state management        │
│ • Audio input switching      │ • Theme rendering            │
└──────────────────────────────┴──────────────────────────────┘
                    │                      │
                    └──── Shared Queue ────┘
                     (FFT Results Buffer)
```

### Hardware Components

| Component | Model | Interface | Purpose |
|-----------|-------|-----------|---------|
| Microcontroller | Raspberry Pi Pico W | - | Main processor with WiFi/BT |
| Display | ILI9341 2.8" TFT | SPI | 320x240 16-bit color display |
| Touch Controller | XPT2046 | SPI | Resistive touch input |
| Microphone | MAX4466 | ADC | Electret mic with gain control |
| Audio Input | 3.5mm Jack | ADC | Line-level audio input |
| Analog Switch | CD4066/TS5A3159 | GPIO | Switch between mic and jack |

### Pin Assignment Plan

```
Pico W GPIO Assignments:
├── Display (SPI0)
│   ├── GP16 - MISO (not used for ILI9341)
│   ├── GP17 - CS (Chip Select)
│   ├── GP18 - SCK (Clock)
│   ├── GP19 - MOSI (Data)
│   ├── GP20 - DC (Data/Command)
│   └── GP21 - RST (Reset)
│
├── Touch Controller (SPI1 or shared SPI0)
│   ├── GP12 - MISO
│   ├── GP13 - CS
│   ├── GP14 - SCK
│   ├── GP15 - MOSI
│   └── GP22 - IRQ (Interrupt)
│
├── Audio Input
│   ├── GP26 (ADC0) - Microphone input
│   ├── GP27 (ADC1) - 3.5mm jack input
│   └── GP10 - Input select (High=Mic, Low=Jack)
│
└── Status/Debug
    ├── LED (GPIO 25/WL_GPIO0) - Activity indicator
    └── GP0/GP1 - UART TX/RX for debug
```

## 🛠️ Development Setup

### Prerequisites

- **Docker Desktop for Mac** (installed and running)
- **USB cable** for Pico programming
- **Git** for version control

### Quick Start with Docker

This project uses a containerized development environment to avoid polluting your system with toolchains and dependencies.

1. **Clone and navigate to the project:**
   ```bash
   cd /Users/dwestbury/Documents/Source\ Code/RPI_Pico/pico_spec_analyzer
   ```

2. **Build the Docker development environment:**
   ```bash
   docker-compose build
   ```

3. **Compile the project:**
   ```bash
   docker-compose run --rm build
   ```

4. **Flash to Pico:**
   - Hold BOOTSEL button on Pico W
   - Connect USB cable to Mac
   - Release BOOTSEL (appears as USB drive)
   - Copy `build/pico_spec_analyzer.uf2` to the Pico drive

### Development Workflow

```bash
# Start interactive development shell
docker-compose run --rm dev

# Inside container:
mkdir -p build && cd build
cmake ..
make -j4

# Or use the convenience script (from host):
./scripts/build.sh

# Clean build:
./scripts/build.sh clean
```

### Manual Build (Native, if preferred)

If you prefer to install tools natively on macOS:

```bash
# Install ARM toolchain and CMake
brew install cmake
brew tap ArmMbed/homebrew-formulae
brew install arm-none-eabi-gcc

# Clone Pico SDK
git clone https://github.com/raspberrypi/pico-sdk.git ~/pico-sdk
cd ~/pico-sdk
git submodule update --init
export PICO_SDK_PATH=~/pico-sdk

# Build
mkdir build && cd build
cmake ..
make -j4
```

## 📦 Project Structure

```
pico_spec_analyzer/
├── src/
│   ├── main.c                 # Entry point, core initialization
│   ├── audio/
│   │   ├── adc_sampler.c      # PIO-based ADC sampling
│   │   ├── fft_processor.c    # FFT computation & band extraction
│   │   └── audio_input.c      # Input switching logic
│   ├── display/
│   │   ├── ili9341.c          # Display driver
│   │   ├── spi_dma.c          # SPI with DMA transfers
│   │   └── themes/
│   │       ├── bars.c         # Bar graph visualization
│   │       ├── waterfall.c    # Waterfall spectrogram
│   │       ├── radial.c       # Circular spectrum
│   │       └── vu_meter.c     # VU meter style
│   ├── touch/
│   │   ├── xpt2046.c          # Touch controller driver
│   │   └── touch_ui.c         # Touch gesture handling
│   └── utils/
│       ├── ring_buffer.c      # Lock-free audio buffer
│       └── dsp_utils.c        # DSP helper functions
│
├── include/
│   ├── config.h               # Pin definitions & constants
│   ├── audio/
│   ├── display/
│   ├── touch/
│   └── utils/
│
├── pio/
│   └── adc_sampler.pio        # PIO assembly for ADC sampling
│
├── lib/
│   └── CMSIS-DSP/             # ARM DSP library (submodule)
│
├── tests/
│   ├── test_fft.c             # FFT accuracy tests
│   └── test_display.c         # Display rendering tests
│
├── scripts/
│   ├── build.sh               # Build helper script
│   ├── flash.sh               # Flash helper script
│   └── monitor.sh             # Serial monitor script
│
├── docker/
│   ├── Dockerfile             # Development container
│   └── entrypoint.sh          # Container startup script
│
├── CMakeLists.txt             # Root CMake configuration
├── docker-compose.yml         # Docker compose configuration
├── .dockerignore              # Docker build exclusions
├── .gitignore                 # Git exclusions
├── .gitmodules                # Git submodules (Pico SDK, CMSIS-DSP)
└── README.md                  # This file
```

## 🎨 Visualization Themes

### 1. Classic Bars
Vertical bars representing each frequency band with peak hold indicators.

### 2. Waterfall Spectrogram
Scrolling time-frequency display showing spectrum history.

### 3. Radial Spectrum
Circular visualization with bands radiating from center.

### 4. VU Meter
Analog-style VU meter with smooth needle animation.

### 5. Mirror Mode
Symmetric mirrored bars for stereo-like effect.

## 🔧 Configuration

### Compile-Time Options (`include/config.h`)

```c
// Audio Configuration
#define SAMPLE_RATE_HZ      22050    // 8000, 16000, 22050
#define FFT_SIZE            64       // Must be power of 2
#define DEFAULT_BAND_COUNT  16       // 4, 8, 16, or 32

// Display Configuration
#define DISPLAY_WIDTH       320
#define DISPLAY_HEIGHT      240
#define DISPLAY_ROTATION    1        // 0, 1, 2, or 3
#define TARGET_FPS          30

// Audio Input
#define MIC_GAIN_DEFAULT    50       // 0-100%
#define AUTO_GAIN_ENABLED   true
#define INPUT_DEFAULT       INPUT_MIC
```

### Runtime Settings (via Touch UI)

- Band count selection
- Visualization theme
- Microphone/Line input toggle
- Gain adjustment
- Auto-gain enable/disable
- Color scheme selection

## 🧪 Testing

```bash
# Run unit tests (in Docker)
docker-compose run --rm test

# Hardware test mode (uploads test firmware)
./scripts/flash.sh test

# Monitor serial output
./scripts/monitor.sh
```

## 📊 Performance Targets

- **Audio Latency**: < 1ms (input to FFT processing)
- **Display Refresh**: 30 FPS minimum
- **FFT Rate**: 50+ FFTs per second
- **Touch Response**: < 100ms
- **CPU Utilization**: < 70% (leaving headroom)

## 🚀 Future Enhancements

- [ ] microSD card for recording FFT data
- [ ] WiFi web interface for remote configuration
- [ ] Bluetooth control via mobile app
- [ ] USB audio class device (use Pico as USB sound card)
- [ ] WS2812 LED ring for ambient visualization
- [ ] Battery power support with LiPo
- [ ] Multiple color schemes/palettes
- [ ] Frequency band labels on display
- [ ] Export screenshots via WiFi

## 📚 References

- [Raspberry Pi Pico Datasheet](./datasheets-and-manuals/Datasheet_RP-008307-DS-1-pico.pdf)
- [ILI9341 Display Datasheet](./datasheets-and-manuals/Datasheet_ILI9341.pdf)
- [MAX4466 Microphone Datasheet](./datasheets-and-manuals/Datasheet_MAX4466.pdf)
- [XPT2046 Touch Controller Datasheet](./datasheets-and-manuals/Datasheet_XPT2046.pdf)
- [Pico C/C++ SDK Documentation](https://www.raspberrypi.com/documentation/pico-sdk/)
- [ARM CMSIS-DSP Library](https://github.com/ARM-software/CMSIS-DSP)
- [PIO Assembly Guide](https://datasheets.raspberrypi.com/pico/raspberry-pi-pico-c-sdk.pdf)

