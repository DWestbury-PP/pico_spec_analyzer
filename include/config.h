/**
 * @file config.h
 * @brief Hardware pins, clocks and system-wide constants.
 *
 * Audio/DSP geometry lives in audio/spectrum.h; this file is hardware only.
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <stdio.h>
#include "pico/stdlib.h"

// ============================================================================
// CLOCKS
// ============================================================================

// 250 MHz core at 1.15 V. clk_peri follows clk_sys (see CMakeLists.txt), so the
// display SPI can run at clk_peri / 4 = 62.5 MHz.
#define SYS_CLOCK_KHZ       250000
#define SYS_VREG_VOLTAGE    VREG_VOLTAGE_1_15

// ============================================================================
// DISPLAY (ILI9341, SPI0)
// ============================================================================

#define DISPLAY_SPI_PORT    spi0
#define DISPLAY_PIN_MISO    16
#define DISPLAY_PIN_CS      17
#define DISPLAY_PIN_SCK     18
#define DISPLAY_PIN_MOSI    19
#define DISPLAY_PIN_DC      20
#define DISPLAY_PIN_RST     21
#define DISPLAY_PIN_BL      22  // backlight (may be hard-wired to 3.3 V)

#define DISPLAY_WIDTH       320
#define DISPLAY_HEIGHT      240
#define DISPLAY_ROTATION    1   // landscape

#define DISPLAY_SPI_INIT_HZ   (20 * 1000 * 1000)    // register setup
#define DISPLAY_SPI_STREAM_HZ (62500 * 1000)        // pixel streaming

// ============================================================================
// TOUCH (XPT2046, SPI1)
// ============================================================================

#define TOUCH_SPI_PORT      spi1
#define TOUCH_PIN_MISO      12
#define TOUCH_PIN_CS        13
#define TOUCH_PIN_SCK       14
#define TOUCH_PIN_MOSI      15
#define TOUCH_PIN_IRQ       11  // active low
#define TOUCH_SPI_SPEED     (2 * 1000 * 1000)

#define TOUCH_HOLD_TIME_MS      1000
#define SWIPE_THRESHOLD_PX      50
#define SWIPE_TIMEOUT_MS        500

// ============================================================================
// AUDIO INPUT
// ============================================================================

#define AUDIO_ADC_MIC       0   // ADC0 = GP26 (MAX4466)
#define AUDIO_PIN_MIC       26

// ============================================================================
// DEBUG
// ============================================================================

#define DEBUG_ENABLE        1
#if DEBUG_ENABLE
    #define DEBUG_PRINTF(...) printf(__VA_ARGS__)
#else
    #define DEBUG_PRINTF(...)
#endif

// ============================================================================
// HELPERS
// ============================================================================

#define RGB565(r, g, b) ((uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3)))

#endif // CONFIG_H
