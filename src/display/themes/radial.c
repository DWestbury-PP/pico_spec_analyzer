/**
 * @file radial.c
 * @brief Radial spectrum visualization implementation
 * 
 * Displays frequency bands as bars arranged in a circle radiating from center.
 */

#include "display/themes/radial.h"
#include "display/ili9341.h"
#include "config.h"
#include <math.h>
#include <string.h>

// ============================================================================
// Configuration
// ============================================================================

#define MAX_BANDS 32
#define CENTER_X (DISPLAY_WIDTH / 2)
#define CENTER_Y (DISPLAY_HEIGHT / 2)
#define MIN_RADIUS 30    // Inner circle radius
#define MAX_RADIUS 110   // Maximum bar length

// ============================================================================
// Private State
// ============================================================================

static float _prev_bands[MAX_BANDS];
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
    
    // Color gradient: Blue → Cyan → Green → Yellow → Red
    if (amplitude < 0.25f) {
        // Blue to Cyan
        float t = amplitude / 0.25f;
        uint8_t g = (uint8_t)(t * 255.0f);
        return RGB565(0, g, 255);
    } else if (amplitude < 0.5f) {
        // Cyan to Green
        float t = (amplitude - 0.25f) / 0.25f;
        uint8_t b = (uint8_t)((1.0f - t) * 255.0f);
        return RGB565(0, 255, b);
    } else if (amplitude < 0.75f) {
        // Green to Yellow
        float t = (amplitude - 0.5f) / 0.25f;
        uint8_t r = (uint8_t)(t * 255.0f);
        return RGB565(r, 255, 0);
    } else {
        // Yellow to Red
        float t = (amplitude - 0.75f) / 0.25f;
        uint8_t g = (uint8_t)((1.0f - t) * 255.0f);
        return RGB565(255, g, 0);
    }
}

// ============================================================================
// Drawing Helpers
// ============================================================================

/**
 * @brief Draw a line using Bresenham's algorithm
 */
static void draw_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
    int16_t dx = abs(x1 - x0);
    int16_t dy = abs(y1 - y0);
    int16_t sx = (x0 < x1) ? 1 : -1;
    int16_t sy = (y0 < y1) ? 1 : -1;
    int16_t err = dx - dy;
    
    while (1) {
        // Draw pixel (with bounds checking)
        if (x0 >= 0 && x0 < DISPLAY_WIDTH && y0 >= 0 && y0 < DISPLAY_HEIGHT) {
            ili9341_draw_pixel(x0, y0, color);
        }
        
        if (x0 == x1 && y0 == y1) break;
        
        int16_t e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

/**
 * @brief Draw a thick line (multiple parallel lines)
 */
static void draw_thick_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, 
                            uint16_t color, uint8_t thickness) {
    for (int8_t offset = -(thickness/2); offset <= thickness/2; offset++) {
        // Calculate perpendicular offset
        float angle = atan2f(y1 - y0, x1 - x0);
        float perp_angle = angle + M_PI / 2.0f;
        int16_t dx = (int16_t)(offset * cosf(perp_angle));
        int16_t dy = (int16_t)(offset * sinf(perp_angle));
        
        draw_line(x0 + dx, y0 + dy, x1 + dx, y1 + dy, color);
    }
}

// ============================================================================
// Public API
// ============================================================================

void radial_init(void) {
    memset(_prev_bands, 0, sizeof(_prev_bands));
    _num_bands = 0;
}

void radial_render(const float *bands, uint8_t num_bands) {
    if (!bands || num_bands == 0 || num_bands > MAX_BANDS) return;
    
    _num_bands = num_bands;
    
    // Clear screen
    ili9341_fill_screen(ILI9341_BLACK);
    
    // Draw center circle
    for (int16_t r = MIN_RADIUS - 2; r <= MIN_RADIUS; r++) {
        for (int16_t angle = 0; angle < 360; angle += 2) {
            float rad = angle * M_PI / 180.0f;
            int16_t x = CENTER_X + (int16_t)(r * cosf(rad));
            int16_t y = CENTER_Y + (int16_t)(r * sinf(rad));
            if (x >= 0 && x < DISPLAY_WIDTH && y >= 0 && y < DISPLAY_HEIGHT) {
                ili9341_draw_pixel(x, y, RGB565(50, 50, 50));
            }
        }
    }
    
    // Draw each frequency band as a bar radiating from center
    for (uint8_t i = 0; i < num_bands; i++) {
        // Smooth transition
        float smoothed = _prev_bands[i] * 0.7f + bands[i] * 0.3f;
        _prev_bands[i] = smoothed;
        
        // Calculate angle for this band
        float angle = (360.0f * i / num_bands) * M_PI / 180.0f;
        
        // Calculate bar length
        float bar_length = smoothed * (MAX_RADIUS - MIN_RADIUS);
        
        // Start and end points
        int16_t x_start = CENTER_X + (int16_t)(MIN_RADIUS * cosf(angle));
        int16_t y_start = CENTER_Y + (int16_t)(MIN_RADIUS * sinf(angle));
        int16_t x_end = CENTER_X + (int16_t)((MIN_RADIUS + bar_length) * cosf(angle));
        int16_t y_end = CENTER_Y + (int16_t)((MIN_RADIUS + bar_length) * sinf(angle));
        
        // Get color based on amplitude
        uint16_t color = amplitude_to_color(smoothed);
        
        // Draw the bar (thickness based on number of bands)
        uint8_t thickness = (num_bands <= 8) ? 5 : (num_bands <= 16) ? 3 : 2;
        draw_thick_line(x_start, y_start, x_end, y_end, color, thickness);
    }
}

void radial_clear(void) {
    ili9341_fill_screen(ILI9341_BLACK);
    radial_init();
}

