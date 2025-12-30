/**
 * @file theme_manager.h
 * @brief Visualization theme management and switching
 * 
 * Manages multiple visualization themes and handles switching between them.
 */

#ifndef THEME_MANAGER_H
#define THEME_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

// ============================================================================
// Theme Types
// ============================================================================

typedef enum {
    THEME_BARS = 0,
    THEME_WATERFALL,
    THEME_RADIAL,
    THEME_MIRROR,
    THEME_COUNT  // Total number of themes
} theme_type_t;

// ============================================================================
// Public API
// ============================================================================

/**
 * @brief Initialize theme manager
 */
void theme_manager_init(void);

/**
 * @brief Get current theme
 * @return Current theme type
 */
theme_type_t theme_manager_get_current(void);

/**
 * @brief Set theme by type
 * @param theme Theme to activate
 */
void theme_manager_set_theme(theme_type_t theme);

/**
 * @brief Switch to next theme
 */
void theme_manager_next(void);

/**
 * @brief Switch to previous theme
 */
void theme_manager_prev(void);

/**
 * @brief Get current theme name
 * @return String name of current theme
 */
const char* theme_manager_get_name(void);

/**
 * @brief Render current theme
 * @param bands Array of frequency band amplitudes (0.0 to 1.0)
 * @param num_bands Number of frequency bands
 */
void theme_manager_render(const float *bands, uint8_t num_bands);

/**
 * @brief Show theme name overlay on screen
 * @param duration_ms How long to show the overlay (milliseconds)
 */
void theme_manager_show_name(uint32_t duration_ms);

/**
 * @brief Update theme name overlay (call every frame)
 * Automatically hides overlay after duration expires
 */
void theme_manager_update_overlay(void);

#endif // THEME_MANAGER_H

