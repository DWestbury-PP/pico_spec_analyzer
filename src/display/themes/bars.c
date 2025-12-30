/**
 * @file bars.c
 * @brief Classic bar graph spectrum visualization
 * 
 * Features:
 * - Vertical bars for each frequency band
 * - Color gradient: green → yellow → red based on amplitude
 * - Peak hold indicators
 * - Smooth animations with decay
 */

#include "display/themes/bars.h"
#include "display/ili9341.h"
#include "config.h"
#include <string.h>

// ============================================================================
// Private State
// ============================================================================

#define MAX_BANDS 32

static float _current_levels[MAX_BANDS] = {0};
static float _peak_levels[MAX_BANDS] = {0};
static uint32_t _peak_hold_time[MAX_BANDS] = {0};

// ============================================================================
// Color Gradient
// ============================================================================

/**
 * @brief Calculate color based on amplitude (0.0 to 1.0)
 * Green (low) → Yellow (mid) → Red (high)
 */
static uint16_t get_amplitude_color(float amplitude) {
    if (amplitude < 0.5f) {
        // Green to Yellow: 0.0 - 0.5
        float t = amplitude * 2.0f;  // 0.0 to 1.0
        uint8_t r = (uint8_t)(t * 255.0f);
        uint8_t g = 255;
        uint8_t b = 0;
        return RGB565(r, g, b);
    } else {
        // Yellow to Red: 0.5 - 1.0
        float t = (amplitude - 0.5f) * 2.0f;  // 0.0 to 1.0
        uint8_t r = 255;
        uint8_t g = (uint8_t)((1.0f - t) * 255.0f);
        uint8_t b = 0;
        return RGB565(r, g, b);
    }
}

// ============================================================================
// Public API
// ============================================================================

void bars_init(void) {
    // Reset state
    memset(_current_levels, 0, sizeof(_current_levels));
    memset(_peak_levels, 0, sizeof(_peak_levels));
    memset(_peak_hold_time, 0, sizeof(_peak_hold_time));
}

void bars_render(const float *bands, uint8_t num_bands) {
    if (!bands || num_bands == 0 || num_bands > MAX_BANDS) return;
    
    uint16_t display_width = ili9341_width();
    uint16_t display_height = ili9341_height();
    
    // Calculate bar dimensions
    uint16_t total_bar_width = display_width - 20;  // 10px margin on each side
    uint16_t bar_spacing = 2;
    uint16_t bar_width = (total_bar_width - (num_bands - 1) * bar_spacing) / num_bands;
    uint16_t bar_max_height = display_height - 40;  // Leave room for margins
    uint16_t bar_bottom = display_height - 10;
    
    // Smoothing factor for animations
    const float smooth_up = 0.3f;    // Fast rise
    const float smooth_down = 0.85f;  // Slower decay
    const float peak_decay = 0.99f;   // Very slow peak decay
    const uint32_t peak_hold_frames = 20;  // Hold peak for 20 frames (~0.67s at 30fps)
    
    // Process each band
    for (uint8_t i = 0; i < num_bands; i++) {
        // Smooth the input value
        float target_level = bands[i];
        if (target_level > _current_levels[i]) {
            // Quick rise
            _current_levels[i] += (target_level - _current_levels[i]) * smooth_up;
        } else {
            // Slower decay
            _current_levels[i] *= smooth_down;
        }
        
        // Clamp to 0-1
        if (_current_levels[i] < 0.01f) _current_levels[i] = 0.0f;
        if (_current_levels[i] > 1.0f) _current_levels[i] = 1.0f;
        
        // Update peak hold
        if (_current_levels[i] > _peak_levels[i]) {
            _peak_levels[i] = _current_levels[i];
            _peak_hold_time[i] = peak_hold_frames;
        } else {
            // Decay peak
            if (_peak_hold_time[i] > 0) {
                _peak_hold_time[i]--;
            } else {
                _peak_levels[i] *= peak_decay;
                if (_peak_levels[i] < 0.01f) _peak_levels[i] = 0.0f;
            }
        }
        
        // Calculate bar position
        uint16_t bar_x = 10 + i * (bar_width + bar_spacing);
        
        // Calculate bar height based on level
        uint16_t bar_height = (uint16_t)(_current_levels[i] * bar_max_height);
        uint16_t bar_y = bar_bottom - bar_height;
        
        // Clear the column (draw background)
        ili9341_fill_rect(bar_x, 10, bar_width, bar_max_height, ILI9341_BLACK);
        
        // Draw the bar with color gradient
        if (bar_height > 0) {
            // Draw bar in segments for gradient effect
            const uint16_t segment_height = 10;
            uint16_t segments = (bar_height + segment_height - 1) / segment_height;
            
            for (uint16_t seg = 0; seg < segments; seg++) {
                uint16_t seg_y = bar_bottom - (seg + 1) * segment_height;
                uint16_t seg_h = segment_height;
                
                // Adjust last segment
                if (seg == segments - 1) {
                    seg_h = bar_height - seg * segment_height;
                    seg_y = bar_y;
                }
                
                // Calculate color for this segment
                float seg_amplitude = (float)(seg + 1) / segments * _current_levels[i];
                uint16_t seg_color = get_amplitude_color(seg_amplitude);
                
                ili9341_fill_rect(bar_x, seg_y, bar_width, seg_h, seg_color);
            }
        }
        
        // Draw peak hold indicator
        if (_peak_levels[i] > 0.05f) {
            uint16_t peak_y = bar_bottom - (uint16_t)(_peak_levels[i] * bar_max_height);
            uint16_t peak_color = get_amplitude_color(_peak_levels[i]);
            ili9341_fill_rect(bar_x, peak_y, bar_width, 2, peak_color);
        }
    }
}

void bars_clear(void) {
    // Fill screen with background color
    ili9341_fill_screen(ILI9341_BLACK);
    
    // Reset internal state
    bars_init();
}

