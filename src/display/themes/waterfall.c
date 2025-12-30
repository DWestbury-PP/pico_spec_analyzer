/**
 * @file waterfall.c
 * @brief Waterfall spectrogram implementation
 * 
 * Displays spectrum as scrolling color-coded history.
 * Each new frame is drawn at the top and scrolls down.
 */

#include "display/themes/waterfall.h"
#include "display/ili9341.h"
#include "config.h"
#include <string.h>

// ============================================================================
// Configuration
// ============================================================================

#define MAX_BANDS 32
#define HISTORY_HEIGHT 200  // Number of rows of history to keep

// ============================================================================
// Private State
// ============================================================================

static uint16_t _history_buffer[HISTORY_HEIGHT][MAX_BANDS];
static uint8_t _current_row = 0;
static uint8_t _num_bands = 0;

// ============================================================================
// Color Mapping
// ============================================================================

/**
 * @brief Convert amplitude to color (blue → cyan → green → yellow → red)
 * Creates a "heat map" effect
 */
static uint16_t amplitude_to_color(float amplitude) {
    if (amplitude < 0.0f) amplitude = 0.0f;
    if (amplitude > 1.0f) amplitude = 1.0f;
    
    // Color gradient: Black → Blue → Cyan → Green → Yellow → Red
    if (amplitude < 0.2f) {
        // Black to Blue
        float t = amplitude / 0.2f;
        uint8_t b = (uint8_t)(t * 255.0f);
        return RGB565(0, 0, b);
    } else if (amplitude < 0.4f) {
        // Blue to Cyan
        float t = (amplitude - 0.2f) / 0.2f;
        uint8_t g = (uint8_t)(t * 255.0f);
        return RGB565(0, g, 255);
    } else if (amplitude < 0.6f) {
        // Cyan to Green
        float t = (amplitude - 0.4f) / 0.2f;
        uint8_t b = (uint8_t)((1.0f - t) * 255.0f);
        return RGB565(0, 255, b);
    } else if (amplitude < 0.8f) {
        // Green to Yellow
        float t = (amplitude - 0.6f) / 0.2f;
        uint8_t r = (uint8_t)(t * 255.0f);
        return RGB565(r, 255, 0);
    } else {
        // Yellow to Red
        float t = (amplitude - 0.8f) / 0.2f;
        uint8_t g = (uint8_t)((1.0f - t) * 255.0f);
        return RGB565(255, g, 0);
    }
}

// ============================================================================
// Public API
// ============================================================================

void waterfall_init(void) {
    memset(_history_buffer, 0, sizeof(_history_buffer));
    _current_row = 0;
    _num_bands = 0;
}

void waterfall_render(const float *bands, uint8_t num_bands) {
    if (!bands || num_bands == 0 || num_bands > MAX_BANDS) return;
    
    _num_bands = num_bands;
    
    uint16_t display_width = ili9341_width();
    uint16_t display_height = ili9341_height();
    
    // Calculate band width
    uint16_t band_width = display_width / num_bands;
    
    // Store new data in history buffer
    for (uint8_t i = 0; i < num_bands; i++) {
        uint16_t color = amplitude_to_color(bands[i]);
        _history_buffer[_current_row][i] = color;
    }
    
    // Scroll display: draw history from oldest to newest
    uint16_t y = 0;
    for (uint16_t row = 0; row < HISTORY_HEIGHT && y < display_height; row++) {
        // Calculate which history row to draw (wrap around circular buffer)
        uint16_t history_idx = (_current_row + 1 + row) % HISTORY_HEIGHT;
        
        // Draw each band for this row
        for (uint8_t band = 0; band < num_bands; band++) {
            uint16_t x = band * band_width;
            uint16_t color = _history_buffer[history_idx][band];
            
            // Draw a single pixel row (or small rectangle for visibility)
            ili9341_fill_rect(x, y, band_width, 1, color);
        }
        
        y++;
    }
    
    // Advance to next row
    _current_row = (_current_row + 1) % HISTORY_HEIGHT;
}

void waterfall_clear(void) {
    ili9341_fill_screen(ILI9341_BLACK);
    waterfall_init();
}

