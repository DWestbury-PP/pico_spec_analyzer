/**
 * @file main.c
 * @brief Main entry point for Pico Spectrum Analyzer
 * 
 * This file initializes the hardware, sets up dual-core operation,
 * and coordinates audio processing with display rendering.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/binary_info.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "hardware/spi.h"
#include "hardware/dma.h"

#include "config.h"

// TODO: Include module headers as they are implemented
// #include "audio/adc_sampler.h"
// #include "audio/fft_processor.h"
// #include "audio/audio_input.h"
// #include "display/ili9341.h"
// #include "display/spi_dma.h"
// #include "touch/xpt2046.h"
// #include "touch/touch_ui.h"

// ============================================================================
// GLOBAL STATE
// ============================================================================

typedef struct {
    audio_input_t input_source;
    uint8_t band_count;
    theme_t current_theme;
    uint8_t mic_gain;
    bool auto_gain;
    bool running;
} app_state_t;

static app_state_t g_app_state = {
    .input_source = INPUT_MIC,
    .band_count = BAND_COUNT_DEFAULT,
    .current_theme = DEFAULT_THEME,
    .mic_gain = MIC_GAIN_DEFAULT,
    .auto_gain = AUTO_GAIN_ENABLED,
    .running = true
};

// ============================================================================
// HARDWARE INITIALIZATION
// ============================================================================

/**
 * @brief Initialize GPIO pins for status LED and audio input selection
 */
static void init_gpio(void) {
    // Initialize LED on GPIO pin (works for both Pico and Pico W)
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_put(LED_PIN, 1);  // Turn on during init
    DEBUG_PRINTF("LED initialized on GPIO %d\n", LED_PIN);
    
    // Initialize audio input selector
    gpio_init(AUDIO_PIN_SELECT);
    gpio_set_dir(AUDIO_PIN_SELECT, GPIO_OUT);
    gpio_put(AUDIO_PIN_SELECT, g_app_state.input_source == INPUT_MIC ? 1 : 0);
    
    DEBUG_PRINTF("GPIO initialized\n");
}

/**
 * @brief Initialize ADC for audio input
 */
static void init_adc(void) {
    adc_init();
    
    // Initialize both ADC inputs (mic and jack)
    adc_gpio_init(AUDIO_PIN_MIC);
    adc_gpio_init(AUDIO_PIN_JACK);
    
    // Select initial input
    adc_select_input(g_app_state.input_source == INPUT_MIC ? AUDIO_ADC_MIC : AUDIO_ADC_JACK);
    
    DEBUG_PRINTF("ADC initialized\n");
}

/**
 * @brief Initialize UART for debug output
 */
static void init_uart(void) {
    uart_init(UART_ID, UART_BAUD_RATE);
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
    
    DEBUG_PRINTF("\n\n=== Pico Spectrum Analyzer ===\n");
    DEBUG_PRINTF("Firmware compiled: %s %s\n", __DATE__, __TIME__);
}

/**
 * @brief Initialize display SPI interface
 */
static void init_display_spi(void) {
    // TODO: Initialize SPI for ILI9341 display
    // This will be implemented in display module
    
    DEBUG_PRINTF("Display SPI initialized (stub)\n");
}

/**
 * @brief Initialize touch controller SPI interface
 */
static void init_touch_spi(void) {
    // TODO: Initialize SPI for XPT2046 touch controller
    // This will be implemented in touch module
    
    DEBUG_PRINTF("Touch SPI initialized (stub)\n");
}

/**
 * @brief Initialize all hardware peripherals
 */
static void hardware_init(void) {
    stdio_init_all();
    init_uart();
    init_gpio();
    init_adc();
    init_display_spi();
    init_touch_spi();
    
    // Blink LED to indicate successful init
    DEBUG_PRINTF("Blinking LED 3 times...\n");
    for (int i = 0; i < 3; i++) {
        gpio_put(LED_PIN, 0);
        sleep_ms(200);
        gpio_put(LED_PIN, 1);
        sleep_ms(200);
    }
    DEBUG_PRINTF("Init complete!\n");
    
    DEBUG_PRINTF("Hardware initialization complete\n");
}

// ============================================================================
// CORE 0: AUDIO PROCESSING
// ============================================================================

/**
 * @brief Core 0 main loop - Audio sampling and FFT processing
 * 
 * This core is dedicated to:
 * - PIO-based ADC sampling at precise intervals
 * - FFT computation using CMSIS-DSP
 * - Frequency band extraction
 * - Audio input switching
 */
static void core0_audio_processing(void) {
    DEBUG_PRINTF("Core 0: Audio processing started\n");
    
    // TODO: Initialize PIO ADC sampler
    // TODO: Initialize FFT processor
    // TODO: Allocate audio buffers
    
    uint32_t frame_count = 0;
    absolute_time_t last_fps_time = get_absolute_time();
    
    while (g_app_state.running) {
        // TODO: Read audio samples from PIO
        // TODO: Apply windowing function
        // TODO: Perform FFT
        // TODO: Extract frequency bands
        // TODO: Update shared buffer for Core 1
        
        frame_count++;
        
        // FPS counter (debug)
        #if DEBUG_PRINT_FPS
        if (absolute_time_diff_us(last_fps_time, get_absolute_time()) >= 1000000) {
            DEBUG_PRINTF("Core 0 FFT rate: %d FFT/s\n", frame_count);
            frame_count = 0;
            last_fps_time = get_absolute_time();
        }
        #endif
        
        // Placeholder: blink LED at 1Hz
        static uint32_t led_counter = 0;
        static bool led_state = false;
        if (++led_counter >= 100) {
            led_state = !led_state;
            gpio_put(LED_PIN, led_state);
            led_counter = 0;
        }
        
        sleep_ms(10);  // TODO: Replace with proper timing from PIO
    }
    
    DEBUG_PRINTF("Core 0: Audio processing stopped\n");
}

// ============================================================================
// CORE 1: DISPLAY AND UI
// ============================================================================

/**
 * @brief Core 1 entry point - launched by multicore_launch_core1()
 * 
 * This core is dedicated to:
 * - Display rendering via SPI + DMA
 * - Touch input handling
 * - UI state management
 * - Theme rendering
 */
static void core1_display_and_ui(void) {
    DEBUG_PRINTF("Core 1: Display and UI started\n");
    
    // TODO: Initialize display driver
    // TODO: Initialize touch controller
    // TODO: Load initial theme
    
    uint32_t frame_count = 0;
    absolute_time_t last_fps_time = get_absolute_time();
    absolute_time_t next_frame_time = get_absolute_time();
    
    while (g_app_state.running) {
        // TODO: Read FFT results from shared buffer
        // TODO: Check touch input
        // TODO: Update UI state
        // TODO: Render current theme
        
        frame_count++;
        
        // FPS counter (debug)
        #if DEBUG_PRINT_FPS
        if (absolute_time_diff_us(last_fps_time, get_absolute_time()) >= 1000000) {
            DEBUG_PRINTF("Core 1 FPS: %d\n", frame_count);
            frame_count = 0;
            last_fps_time = get_absolute_time();
        }
        #endif
        
        // Frame rate limiting
        next_frame_time = delayed_by_us(next_frame_time, FRAME_TIME_US);
        sleep_until(next_frame_time);
    }
    
    DEBUG_PRINTF("Core 1: Display and UI stopped\n");
}

// ============================================================================
// MAIN
// ============================================================================

/**
 * @brief Main entry point
 * 
 * Initializes hardware, launches Core 1 for display/UI,
 * then enters audio processing loop on Core 0.
 */
int main(void) {
    // Initialize hardware
    hardware_init();
    
    // Add binary info (visible in picotool)
    bi_decl(bi_program_description("Pico Spectrum Analyzer - Real-time FFT Audio Visualizer"));
    bi_decl(bi_1pin_with_name(LED_PIN, "Status LED"));
    bi_decl(bi_1pin_with_name(AUDIO_PIN_MIC, "Microphone Input (ADC0)"));
    bi_decl(bi_1pin_with_name(AUDIO_PIN_JACK, "Audio Jack Input (ADC1)"));
    
    DEBUG_PRINTF("\n");
    DEBUG_PRINTF("Configuration:\n");
    DEBUG_PRINTF("  Sample Rate: %d Hz\n", SAMPLE_RATE_HZ);
    DEBUG_PRINTF("  FFT Size: %d\n", FFT_SIZE);
    DEBUG_PRINTF("  Band Count: %d\n", g_app_state.band_count);
    DEBUG_PRINTF("  Display: %dx%d @ %d FPS\n", DISPLAY_WIDTH, DISPLAY_HEIGHT, TARGET_FPS);
    DEBUG_PRINTF("  Theme: %d\n", g_app_state.current_theme);
    DEBUG_PRINTF("\n");
    
    // Launch Core 1 for display and UI
    DEBUG_PRINTF("Launching Core 1...\n");
    multicore_launch_core1(core1_display_and_ui);
    sleep_ms(100);  // Give Core 1 time to start
    
    // Core 0 handles audio processing
    DEBUG_PRINTF("Starting Core 0 audio processing...\n");
    core0_audio_processing();
    
    // Cleanup (should never reach here in normal operation)
    DEBUG_PRINTF("Shutting down...\n");
    multicore_reset_core1();
    
    return 0;
}

