/**
 * @file config.h
 * @brief Main configuration file for Pico Spectrum Analyzer
 * 
 * This file contains all hardware pin definitions, compile-time configuration
 * options, and system-wide constants.
 */

#ifndef CONFIG_H
#define CONFIG_H

#include "pico/stdlib.h"

// ============================================================================
// HARDWARE CONFIGURATION
// ============================================================================

// --- Display Configuration (ILI9341 SPI) ---
#define DISPLAY_SPI_PORT    spi0
#define DISPLAY_PIN_MISO    16  // Not used but defined for SPI
#define DISPLAY_PIN_CS      17  // Chip Select
#define DISPLAY_PIN_SCK     18  // Clock
#define DISPLAY_PIN_MOSI    19  // Data Out
#define DISPLAY_PIN_DC      20  // Data/Command Select
#define DISPLAY_PIN_RST     21  // Reset
#define DISPLAY_PIN_BL      22  // Backlight (optional, can be tied to 3.3V)

#define DISPLAY_WIDTH       320
#define DISPLAY_HEIGHT      240
#define DISPLAY_ROTATION    1   // 0=0°, 1=90°, 2=180°, 3=270°
#define DISPLAY_SPI_SPEED   (32 * 1000 * 1000)  // 32 MHz

// --- Touch Controller Configuration (XPT2046) ---
#define TOUCH_SPI_PORT      spi1
#define TOUCH_PIN_MISO      12  // Data In (GP12, Pin 16)
#define TOUCH_PIN_CS        13  // Chip Select (GP13, Pin 17)
#define TOUCH_PIN_SCK       14  // Clock (GP14, Pin 19)
#define TOUCH_PIN_MOSI      15  // Data Out (GP15, Pin 20)
#define TOUCH_PIN_IRQ       11  // Interrupt - active low (GP11, Pin 15)
#define TOUCH_SPI_SPEED     (2 * 1000 * 1000)  // 2 MHz

// Note: GP23-25,29 are used internally by CYW43 wireless on Pico W

// --- Audio Input Configuration ---
#define AUDIO_ADC_MIC       0   // ADC channel 0 (GP26)
#define AUDIO_ADC_JACK      1   // ADC channel 1 (GP27)
#define AUDIO_PIN_MIC       26  // Microphone input
#define AUDIO_PIN_JACK      27  // 3.5mm jack input
#define AUDIO_PIN_SELECT    10  // Input selector (High=Mic, Low=Jack)

// --- Status LED ---
// Note: GP15 is used by touch controller (MOSI)
// For status LED, we'll use USB serial output instead of physical LED
// Or you can use any other free GPIO pin (e.g., GP11, GP28)
// #define LED_PIN         11  // External LED (optional)

// --- Debug UART ---
#define UART_ID             uart0
#define UART_TX_PIN         0
#define UART_RX_PIN         1
#define UART_BAUD_RATE      115200

// ============================================================================
// AUDIO PROCESSING CONFIGURATION
// ============================================================================

// --- Sampling Configuration ---
#define SAMPLE_RATE_HZ      22050   // Audio sample rate (8000, 16000, 22050)
#define FFT_SIZE            64      // Must be power of 2 (64, 128, 256)
#define FFT_OVERLAP         0.5f    // 50% overlap between FFT windows

// Derived values
#define SAMPLES_PER_FFT     FFT_SIZE
#define FFT_RATE_HZ         (SAMPLE_RATE_HZ / (FFT_SIZE * (1.0f - FFT_OVERLAP)))
#define NYQUIST_FREQ_HZ     (SAMPLE_RATE_HZ / 2)

// --- Frequency Band Configuration ---
#define BAND_COUNT_MIN      4
#define BAND_COUNT_MAX      32
#define BAND_COUNT_DEFAULT  16

// Frequency range for analysis
#define FREQ_MIN_HZ         100     // Low frequency cutoff
#define FREQ_MAX_HZ         11000   // High frequency cutoff (below Nyquist)

// --- Audio Processing Options ---
#define WINDOW_FUNCTION     WINDOW_HANN     // HANN, HAMMING, BLACKMAN
#define AUTO_GAIN_ENABLED   true            // Enable automatic gain control
#define MIC_GAIN_DEFAULT    50              // 0-100%
#define PEAK_DECAY_RATE     0.95f           // Peak hold decay factor per frame
#define PEAK_HOLD_MS        1500            // Peak hold time in milliseconds
#define SMOOTHING_FACTOR    0.7f            // Temporal smoothing (0=none, 1=full)

// FFT visualization gain (increase if bars are too small)
// Typical values: 5.0 (high gain) to 50.0 (low gain)
#define FFT_DISPLAY_GAIN    5.0f            // Lower = more sensitive

// ============================================================================
// DISPLAY CONFIGURATION
// ============================================================================

#define TARGET_FPS          30              // Target refresh rate
#define FRAME_TIME_US       (1000000 / TARGET_FPS)

// Default theme
#define DEFAULT_THEME       THEME_BARS

// Color depth
#define COLOR_DEPTH_16BIT   1

// Color scheme (RGB565 format)
#define COLOR_BACKGROUND    0x0000  // Black
#define COLOR_FOREGROUND    0xFFFF  // White
#define COLOR_BAR_LOW       0x07E0  // Green
#define COLOR_BAR_MED       0xFFE0  // Yellow
#define COLOR_BAR_HIGH      0xF800  // Red
#define COLOR_PEAK          0x001F  // Blue
#define COLOR_TEXT          0xFFFF  // White
#define COLOR_GRID          0x31A6  // Dark gray

// Thresholds for color transitions (0.0 - 1.0)
#define THRESHOLD_MED       0.5f
#define THRESHOLD_HIGH      0.8f

// ============================================================================
// TOUCH UI CONFIGURATION
// ============================================================================

#define TOUCH_DEBOUNCE_MS       50      // Debounce time for touch events
#define TOUCH_HOLD_TIME_MS      1000    // Long press threshold
#define SWIPE_THRESHOLD_PX      50      // Minimum distance for swipe
#define SWIPE_TIMEOUT_MS        500     // Maximum time for swipe gesture

// ============================================================================
// SYSTEM CONFIGURATION
// ============================================================================

// --- Core Assignment ---
#define CORE_AUDIO          0   // Core 0: Audio sampling and FFT
#define CORE_DISPLAY        1   // Core 1: Display rendering and UI

// --- Buffer Sizes ---
#define AUDIO_BUFFER_SIZE   (FFT_SIZE * 4)  // Ring buffer for audio samples
#define FFT_RESULT_BUFFER   2               // Double buffering for FFT results

// --- DMA Configuration ---
#define DMA_CHANNEL_DISPLAY 0
#define DMA_CHANNEL_TOUCH   1

// --- Debug Options ---
#define DEBUG_ENABLE        1
#define DEBUG_PRINT_FPS     0
#define DEBUG_PRINT_FFT     0

#if DEBUG_ENABLE
    #define DEBUG_PRINTF(...) printf(__VA_ARGS__)
#else
    #define DEBUG_PRINTF(...)
#endif

// ============================================================================
// ENUMERATIONS
// ============================================================================

// Audio input source
typedef enum {
    INPUT_MIC = 0,
    INPUT_JACK = 1
} audio_input_t;

// Window functions for FFT
typedef enum {
    WINDOW_NONE = 0,
    WINDOW_HANN,
    WINDOW_HAMMING,
    WINDOW_BLACKMAN
} window_function_t;

// Visualization themes
typedef enum {
    THEME_BARS = 0,
    THEME_WATERFALL,
    THEME_RADIAL,
    THEME_VU_METER,
    THEME_MIRROR,
    THEME_COUNT  // Total number of themes
} theme_t;

// ============================================================================
// HELPER MACROS
// ============================================================================

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define CLAMP(x, min, max) (MIN(MAX((x), (min)), (max)))
#define ABS(x) ((x) < 0 ? -(x) : (x))

// Convert RGB888 to RGB565
#define RGB565(r, g, b) ((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3))

// Map value from one range to another
#define MAP(x, in_min, in_max, out_min, out_max) \
    (((x) - (in_min)) * ((out_max) - (out_min)) / ((in_max) - (in_min)) + (out_min))

#endif // CONFIG_H

