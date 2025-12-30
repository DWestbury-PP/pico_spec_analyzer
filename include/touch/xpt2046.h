/**
 * @file xpt2046.h
 * @brief XPT2046 resistive touch controller driver
 * 
 * Driver for XPT2046 touch controller commonly found on
 * ILI9341 TFT display modules.
 */

#ifndef XPT2046_H
#define XPT2046_H

#include <stdint.h>
#include <stdbool.h>

// ============================================================================
// Touch Point Structure
// ============================================================================

typedef struct {
    uint16_t x;          // X coordinate (raw ADC value)
    uint16_t y;          // Y coordinate (raw ADC value)
    uint16_t pressure;   // Pressure/Z value
    bool is_pressed;     // Touch detected
} touch_point_t;

// ============================================================================
// Gesture Types
// ============================================================================

typedef enum {
    GESTURE_NONE = 0,
    GESTURE_TAP,
    GESTURE_SWIPE_LEFT,
    GESTURE_SWIPE_RIGHT,
    GESTURE_SWIPE_UP,
    GESTURE_SWIPE_DOWN,
    GESTURE_LONG_PRESS
} touch_gesture_t;

// ============================================================================
// Public API
// ============================================================================

/**
 * @brief Initialize XPT2046 touch controller
 * @return true if successful
 */
bool xpt2046_init(void);

/**
 * @brief Read current touch point
 * @param point Output touch point data
 * @return true if touch detected
 */
bool xpt2046_read(touch_point_t *point);

/**
 * @brief Check if screen is currently touched
 * @return true if touched
 */
bool xpt2046_is_touched(void);

/**
 * @brief Calibrate touch coordinates to screen coordinates
 * @param raw_x Raw X coordinate from touch controller
 * @param raw_y Raw Y coordinate from touch controller
 * @param screen_x Output screen X coordinate
 * @param screen_y Output screen Y coordinate
 */
void xpt2046_calibrate(uint16_t raw_x, uint16_t raw_y, 
                       uint16_t *screen_x, uint16_t *screen_y);

/**
 * @brief Detect gesture from touch events
 * @return Detected gesture type
 */
touch_gesture_t xpt2046_detect_gesture(void);

#endif // XPT2046_H

