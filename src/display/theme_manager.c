/**
 * @file theme_manager.c
 * @brief Theme management implementation
 */

#include "display/theme_manager.h"
#include "display/themes/bars.h"
#include "display/themes/waterfall.h"
#include "display/themes/radial.h"
#include "display/themes/mirror.h"
#include "display/ili9341.h"
#include "config.h"
#include "pico/stdlib.h"

// ============================================================================
// Private State
// ============================================================================

static theme_type_t _current_theme = THEME_BARS;
static bool _overlay_visible = false;
static absolute_time_t _overlay_end_time;

static const char* _theme_names[THEME_COUNT] = {
    "Classic Bars",
    "Waterfall",
    "Radial",
    "Mirror Mode"
};

// ============================================================================
// Private Helpers
// ============================================================================

/**
 * @brief Draw theme name overlay
 */
static void draw_overlay(void) {
    const char* name = _theme_names[_current_theme];
    
    // Calculate text dimensions (rough estimate)
    uint16_t text_len = 0;
    for (const char* p = name; *p; p++) text_len++;
    uint16_t text_width = text_len * 12;  // Approximate character width
    uint16_t text_height = 16;
    
    // Center position
    uint16_t x = (ili9341_width() - text_width) / 2;
    uint16_t y = ili9341_height() - 40;
    
    // Draw semi-transparent background (black rectangle)
    uint16_t bg_padding = 10;
    ili9341_fill_rect(x - bg_padding, y - bg_padding, 
                      text_width + 2 * bg_padding, 
                      text_height + 2 * bg_padding, 
                      ILI9341_BLACK);
    
    // Draw border
    ili9341_draw_rect(x - bg_padding, y - bg_padding,
                      text_width + 2 * bg_padding,
                      text_height + 2 * bg_padding,
                      ILI9341_WHITE);
    
    // Draw text (simple implementation - could be enhanced)
    // For now, just draw a filled rectangle as placeholder
    // In a full implementation, you'd use a font rendering library
    ili9341_fill_rect(x, y, text_width, 2, ILI9341_CYAN);
}

// ============================================================================
// Public API
// ============================================================================

void theme_manager_init(void) {
    _current_theme = THEME_BARS;
    _overlay_visible = false;
    
    // Initialize all themes
    bars_init();
    waterfall_init();
    radial_init();
    mirror_init();
    
    DEBUG_PRINTF("Theme manager initialized (default: %s)\n", _theme_names[_current_theme]);
}

theme_type_t theme_manager_get_current(void) {
    return _current_theme;
}

void theme_manager_set_theme(theme_type_t theme) {
    if (theme >= THEME_COUNT) return;
    
    if (_current_theme != theme) {
        _current_theme = theme;
        
        // Clear the display for the new theme
        ili9341_fill_screen(ILI9341_BLACK);
        
        // Reinitialize the new theme
        switch (theme) {
            case THEME_BARS:
                bars_init();
                break;
            case THEME_WATERFALL:
                waterfall_init();
                break;
            case THEME_RADIAL:
                radial_init();
                break;
            case THEME_MIRROR:
                mirror_init();
                break;
            default:
                break;
        }
        
        DEBUG_PRINTF("Switched to theme: %s\n", _theme_names[theme]);
        
        // Show theme name for 2 seconds
        theme_manager_show_name(2000);
    }
}

void theme_manager_next(void) {
    theme_type_t next = (_current_theme + 1) % THEME_COUNT;
    theme_manager_set_theme(next);
}

void theme_manager_prev(void) {
    theme_type_t prev = (_current_theme == 0) ? (THEME_COUNT - 1) : (_current_theme - 1);
    theme_manager_set_theme(prev);
}

const char* theme_manager_get_name(void) {
    return _theme_names[_current_theme];
}

void theme_manager_render(const float *bands, uint8_t num_bands) {
    if (!bands || num_bands == 0) return;
    
    // Render current theme
    switch (_current_theme) {
        case THEME_BARS:
            bars_render(bands, num_bands);
            break;
        case THEME_WATERFALL:
            waterfall_render(bands, num_bands);
            break;
        case THEME_RADIAL:
            radial_render(bands, num_bands);
            break;
        case THEME_MIRROR:
            mirror_render(bands, num_bands);
            break;
        default:
            break;
    }
    
    // Draw overlay if visible
    if (_overlay_visible) {
        draw_overlay();
    }
}

void theme_manager_show_name(uint32_t duration_ms) {
    _overlay_visible = true;
    _overlay_end_time = make_timeout_time_ms(duration_ms);
}

void theme_manager_update_overlay(void) {
    if (_overlay_visible && time_reached(_overlay_end_time)) {
        _overlay_visible = false;
    }
}

