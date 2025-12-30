/**
 * @file spectrum_analyzer.c
 * @brief Real-time spectrum analyzer with live audio input
 * 
 * Captures audio from MAX4466 microphone, performs FFT,
 * and displays spectrum on ILI9341 display.
 */

#include <stdio.h>
#include "pico/stdlib.h"
#include "display/ili9341.h"
#include "display/themes/bars.h"
#include "audio/adc_sampler.h"
#include "audio/fft_processor.h"
#include "config.h"

// ============================================================================
// Configuration
// ============================================================================

#define NUM_BANDS 16        // Number of frequency bands to display
#define TARGET_FPS 30       // Target frames per second
#define FRAME_TIME_US (1000000 / TARGET_FPS)

// ============================================================================
// Main
// ============================================================================

int main(void) {
    // Initialize stdio - wait for USB
    stdio_init_all();
    sleep_ms(2000);
    
    printf("\n\n");
    printf("============================================\n");
    printf("  Real-Time Spectrum Analyzer\n");
    printf("============================================\n\n");
    
    printf("Configuration:\n");
    printf("  Microphone: MAX4466 on GP%d (ADC%d)\n", AUDIO_PIN_MIC, AUDIO_ADC_MIC);
    printf("  Sample Rate: %d Hz\n", SAMPLE_RATE_HZ);
    printf("  FFT Size: %d\n", FFT_SIZE);
    printf("  Bands: %d\n", NUM_BANDS);
    printf("  Target FPS: %d\n", TARGET_FPS);
    printf("  Display: %dx%d\n", DISPLAY_WIDTH, DISPLAY_HEIGHT);
    printf("\n");
    
    // Initialize display
    printf("Initializing display...\n");
    if (!ili9341_init()) {
        printf("ERROR: Display initialization failed!\n");
        return 1;
    }
    
    ili9341_set_rotation(DISPLAY_ROTATION);
    printf("Display ready: %dx%d\n\n", ili9341_width(), ili9341_height());
    
    // Initialize visualization
    printf("Initializing visualization...\n");
    bars_init();
    bars_clear();
    
    // Initialize ADC sampler
    printf("Initializing ADC sampler...\n");
    if (!adc_sampler_init(AUDIO_ADC_MIC, SAMPLE_RATE_HZ)) {
        printf("ERROR: ADC sampler initialization failed!\n");
        return 1;
    }
    
    // Initialize FFT processor
    printf("Initializing FFT processor...\n");
    if (!fft_processor_init(SAMPLE_RATE_HZ)) {
        printf("ERROR: FFT processor initialization failed!\n");
        return 1;
    }
    
    // Start sampling
    printf("Starting audio capture...\n");
    adc_sampler_start();
    
    printf("\n============================================\n");
    printf("  Spectrum Analyzer Running!\n");
    printf("============================================\n\n");
    
    printf("Make some noise! Clap, talk, play music...\n");
    printf("Watch the display for live spectrum visualization!\n\n");
    
    printf("Performance stats will be printed periodically...\n\n");
    
    // Print frequency band ranges
    printf("Frequency bands:\n");
    for (uint8_t i = 0; i < NUM_BANDS; i++) {
        float freq_min, freq_max;
        fft_processor_get_band_range(i, NUM_BANDS, &freq_min, &freq_max);
        printf("  Band %2d: %6.1f - %6.1f Hz\n", i, freq_min, freq_max);
    }
    printf("\n");
    
    // Allocate buffers
    uint16_t audio_samples[FFT_SIZE];
    float frequency_bands[NUM_BANDS];
    
    // Timing variables
    absolute_time_t next_frame_time = get_absolute_time();
    absolute_time_t last_stats_time = get_absolute_time();
    uint32_t frame_count = 0;
    uint32_t frames_since_stats = 0;
    
    uint32_t min_frame_time_us = UINT32_MAX;
    uint32_t max_frame_time_us = 0;
    uint32_t total_frame_time_us = 0;
    uint32_t fft_failures = 0;
    
    // Main loop
    while (true) {
        absolute_time_t frame_start = get_absolute_time();
        
        // Check if we have enough samples for FFT
        if (adc_sampler_available() >= FFT_SIZE) {
            // Read samples
            uint32_t samples_read = adc_sampler_read(audio_samples, FFT_SIZE);
            
            if (samples_read == FFT_SIZE) {
                // Perform FFT and extract frequency bands
                if (fft_processor_compute(audio_samples, frequency_bands, NUM_BANDS)) {
                    // Render visualization
                    bars_render(frequency_bands, NUM_BANDS);
                } else {
                    fft_failures++;
                }
            }
        }
        
        // Calculate frame timing
        absolute_time_t frame_end = get_absolute_time();
        uint32_t frame_time_us = absolute_time_diff_us(frame_start, frame_end);
        
        // Update stats
        if (frame_time_us < min_frame_time_us) min_frame_time_us = frame_time_us;
        if (frame_time_us > max_frame_time_us) max_frame_time_us = frame_time_us;
        total_frame_time_us += frame_time_us;
        
        frame_count++;
        frames_since_stats++;
        
        // Print stats every 5 seconds
        int64_t stats_interval_us = absolute_time_diff_us(last_stats_time, frame_end);
        if (stats_interval_us >= 5000000) {  // 5 seconds
            float actual_fps = (float)frames_since_stats / (stats_interval_us / 1000000.0f);
            float avg_frame_time_ms = (total_frame_time_us / (float)frames_since_stats) / 1000.0f;
            uint32_t samples_available = adc_sampler_available();
            
            printf("Frame %lu | FPS: %.1f | Frame time: %.2f ms (min: %.2f, max: %.2f) | Buffer: %lu samples",
                   frame_count,
                   actual_fps,
                   avg_frame_time_ms,
                   min_frame_time_us / 1000.0f,
                   max_frame_time_us / 1000.0f,
                   samples_available);
            
            if (fft_failures > 0) {
                printf(" | FFT failures: %lu", fft_failures);
            }
            printf("\n");
            
            // Reset stats
            last_stats_time = frame_end;
            frames_since_stats = 0;
            min_frame_time_us = UINT32_MAX;
            max_frame_time_us = 0;
            total_frame_time_us = 0;
            fft_failures = 0;
        }
        
        // Frame rate limiting
        next_frame_time = delayed_by_us(next_frame_time, FRAME_TIME_US);
        
        // Check if we're falling behind
        int64_t time_until_next = absolute_time_diff_us(frame_end, next_frame_time);
        if (time_until_next < 0) {
            // We're behind schedule, skip waiting and catch up
            next_frame_time = frame_end;
        } else {
            // Wait until next frame
            sleep_until(next_frame_time);
        }
    }
    
    // Cleanup (never reached in normal operation)
    adc_sampler_stop();
    
    return 0;
}

