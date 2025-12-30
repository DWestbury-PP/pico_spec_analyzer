/**
 * @file spectrum_viz_test.c
 * @brief Spectrum analyzer visualization test with mock audio
 * 
 * Demonstrates the spectrum visualization with simulated audio data.
 * Tests performance, animations, and visual quality.
 */

#include <stdio.h>
#include "pico/stdlib.h"
#include "display/ili9341.h"
#include "display/themes/bars.h"
#include "utils/mock_audio.h"
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
    printf("  Spectrum Analyzer Visualization Test\n");
    printf("============================================\n\n");
    
    printf("Configuration:\n");
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
    
    // Initialize mock audio generator
    printf("Initializing mock audio generator...\n");
    mock_audio_init();
    printf("Starting with PATTERN_AUTO (cycles through patterns)\n\n");
    
    printf("============================================\n");
    printf("  Visualization Running!\n");
    printf("============================================\n\n");
    
    printf("Watch the display for animated spectrum bars!\n");
    printf("Pattern will change every 5 seconds.\n\n");
    
    printf("Performance stats will be printed periodically...\n\n");
    
    // Allocate buffer for frequency bands
    float bands[NUM_BANDS];
    
    // Timing variables
    absolute_time_t next_frame_time = get_absolute_time();
    absolute_time_t last_stats_time = get_absolute_time();
    uint32_t frame_count = 0;
    uint32_t frames_since_stats = 0;
    
    uint32_t min_frame_time_us = UINT32_MAX;
    uint32_t max_frame_time_us = 0;
    uint32_t total_frame_time_us = 0;
    
    // Main loop
    while (true) {
        absolute_time_t frame_start = get_absolute_time();
        
        // Generate mock audio data
        mock_audio_generate(bands, NUM_BANDS, PATTERN_AUTO);
        
        // Render visualization
        bars_render(bands, NUM_BANDS);
        
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
            
            uint32_t pattern_index = (mock_audio_frame_count() / 150) % 6;
            const char* pattern_name = mock_audio_pattern_name((mock_audio_pattern_t)pattern_index);
            
            printf("Frame %lu | FPS: %.1f | Frame time: %.2f ms (min: %.2f, max: %.2f) | Pattern: %s\n",
                   frame_count,
                   actual_fps,
                   avg_frame_time_ms,
                   min_frame_time_us / 1000.0f,
                   max_frame_time_us / 1000.0f,
                   pattern_name);
            
            // Reset stats
            last_stats_time = frame_end;
            frames_since_stats = 0;
            min_frame_time_us = UINT32_MAX;
            max_frame_time_us = 0;
            total_frame_time_us = 0;
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
    
    return 0;
}

