/**
 * @file mirror.c
 * @brief Mirror mode visualization implementation
 * 
 * Displays frequency bands as vertically mirrored bars from center.
 */

#include "display/themes/mirror.h"
#include "display/ili9341.h"
#include "config.h"
#include <string.h>

// ============================================================================
// Configuration
// ============================================================================

#define MAX_BANDS 32
#define CENTER_Y (DISPLAY_HEIGHT / 2)
#define MAX_BAR_HEIGHT (DISPLAY_HEIGHT / 2 - 5)  // Leave small gap at edges

// ============================================================================
// Private State
// ============================================================================

static float _prev_bands[MAX_BANDS];
static float _peak_levels[MAX_BANDS];
static uint32_t _peak_hold_times[MAX_BANDS];
static uint8_t _num_bands = 0;

// ============================================================================
// Color Mapping
// ============================================================================

/**
 * @brief Convert amplitude to color
 */
static uint16_t amplitude_to_color(float amplitude) {
    if (amplitude < 0.0f) amplitude = 0.0f;
    if (amplitude > 1.0f) amplitude = 1.0f;
    
    // Color gradient: Green → Yellow → Red
    if (amplitude < 0.6f) {
        // Green to Yellow
        float t = amplitude / 0.6f;
        uint8_t r = (uint8_t)(t * 255.0f);
        return RGB565(r, 255, 0);
    } else {
        // Yellow to Red
        float t = (amplitude - 0.6f) / 0.4f;
        uint8_t g = (uint8_t)((1.0f - t) * 255.0f);
        return RGB565(255, g, 0);
    }
}

// ============================================================================
// Public API
// ============================================================================

void mirror_init(void) {
    memset(_prev_bands, 0, sizeof(_prev_bands));
    memset(_peak_levels, 0, sizeof(_peak_levels));
    memset(_peak_hold_times, 0, sizeof(_peak_hold_times));
    _num_bands = 0;
}

void mirror_render(const float *bands, uint8_t num_bands) {
    if (!bands || num_bands == 0 || num_bands > MAX_BANDS) return;
    
    _num_bands = num_bands;
    uint16_t display_width = ili9341_width();
    uint32_t current_time = to_ms_since_boot(get_absolute_time());
    
    // Calculate band width and spacing
    uint16_t band_width = display_width / num_bands;
    if (band_width > 20) band_width = 20;  // Cap width for visual appeal
    uint16_t spacing = 2;
    if (band_width > spacing) band_width -= spacing;
    
    // Calculate starting X position (center bands)
    uint16_t total_width = (band_width + spacing) * num_bands;
    uint16_t start_x = (display_width - total_width) / 2;
    
    // Clear screen
    ili9341_fill_screen(ILI9341_BLACK);
    
    // Draw center line
    ili9341_fill_rect(0, CENTER_Y - 1, display_width, 2, RGB565(50, 50, 50));
    
    // Draw each band mirrored
    for (uint8_t i = 0; i < num_bands; i++) {
        // Smooth the amplitude
        float smoothed = _prev_bands[i] * 0.7f + bands[i] * 0.3f;
        _prev_bands[i] = smoothed;
        
        // Calculate bar height
        uint16_t bar_height = (uint16_t)(smoothed * MAX_BAR_HEIGHT);
        
        // Update peak hold
        if (smoothed > _peak_levels[i]) {
            _peak_levels[i] = smoothed;
            _peak_hold_times[i] = current_time;
        } else if (current_time - _peak_hold_times[i] > PEAK_HOLD_MS) {
            // Decay peak slowly
            _peak_levels[i] *= PEAK_DECAY_RATE;
        }
        
        uint16_t peak_height = (uint16_t)(_peak_levels[i] * MAX_BAR_HEIGHT);
        
        // Calculate X position for this band
        uint16_t x = start_x + i * (band_width + spacing);
        
        // Draw bars in gradient segments (both top and bottom)
        for (uint16_t y = 0; y < bar_height; y++) {
            float level = (float)y / MAX_BAR_HEIGHT;
            uint16_t color = amplitude_to_color(level);
            
            // Top bar (growing upward from center)
            ili9341_fill_rect(x, CENTER_Y - y - 1, band_width, 1, color);
            
            // Bottom bar (growing downward from center)
            ili9341_fill_rect(x, CENTER_Y + y, band_width, 1, color);
        }
        
        // Draw peak indicators
        if (peak_height > bar_height && peak_height > 2) {
            uint16_t peak_color = RGB565(255, 255, 255);  // White peak
            
            // Top peak
            ili9341_fill_rect(x, CENTER_Y - peak_height - 1, band_width, 2, peak_color);
            
            // Bottom peak
            ili9341_fill_rect(x, CENTER_Y + peak_height - 1, band_width, 2, peak_color);
        }
    }
}

void mirror_clear(void) {
    ili9341_fill_screen(ILI9341_BLACK);
    mirror_init();
}

