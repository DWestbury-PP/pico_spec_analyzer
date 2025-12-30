/**
 * @file xpt2046.c
 * @brief XPT2046 touch controller implementation
 */

#include "touch/xpt2046.h"
#include "config.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"

// ============================================================================
// XPT2046 Commands
// ============================================================================

#define XPT2046_CMD_X       0xD0  // Read X position
#define XPT2046_CMD_Y       0x90  // Read Y position
#define XPT2046_CMD_Z1      0xB0  // Read Z1 (pressure)
#define XPT2046_CMD_Z2      0xC0  // Read Z2 (pressure)

// ============================================================================
// Calibration Constants
// ============================================================================

// These will need adjustment based on your specific display
#define TOUCH_X_MIN 200
#define TOUCH_X_MAX 3900
#define TOUCH_Y_MIN 200
#define TOUCH_Y_MAX 3900

// Pressure threshold
#define PRESSURE_THRESHOLD 400

// ============================================================================
// Gesture Detection State
// ============================================================================

static struct {
    bool is_touching;
    uint16_t start_x;
    uint16_t start_y;
    uint16_t last_x;
    uint16_t last_y;
    absolute_time_t touch_start_time;
    absolute_time_t touch_end_time;
} _gesture_state = {0};

// ============================================================================
// Private Functions
// ============================================================================

/**
 * @brief Send command and read 12-bit result
 */
static uint16_t xpt2046_read_data(uint8_t command) {
    uint8_t tx_buf[3] = {command, 0x00, 0x00};
    uint8_t rx_buf[3] = {0};
    
    gpio_put(TOUCH_PIN_CS, 0);  // Select touch controller
    spi_write_read_blocking(TOUCH_SPI_PORT, tx_buf, rx_buf, 3);
    gpio_put(TOUCH_PIN_CS, 1);  // Deselect
    
    // Combine 12-bit result from bytes 1 and 2
    uint16_t result = ((rx_buf[1] << 8) | rx_buf[2]) >> 3;
    return result & 0x0FFF;
}

// ============================================================================
// Public API
// ============================================================================

bool xpt2046_init(void) {
    // Initialize SPI for touch controller
    spi_init(TOUCH_SPI_PORT, TOUCH_SPI_SPEED);
    spi_set_format(TOUCH_SPI_PORT, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    
    // Set up GPIO pins
    gpio_set_function(TOUCH_PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(TOUCH_PIN_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(TOUCH_PIN_MISO, GPIO_FUNC_SPI);
    
    // CS pin
    gpio_init(TOUCH_PIN_CS);
    gpio_set_dir(TOUCH_PIN_CS, GPIO_OUT);
    gpio_put(TOUCH_PIN_CS, 1);  // Deselect
    
    // IRQ pin (optional, active low when touched)
    gpio_init(TOUCH_PIN_IRQ);
    gpio_set_dir(TOUCH_PIN_IRQ, GPIO_IN);
    gpio_pull_up(TOUCH_PIN_IRQ);
    
    // Initialize gesture state
    _gesture_state.is_touching = false;
    
    DEBUG_PRINTF("XPT2046 touch controller initialized\n");
    return true;
}

bool xpt2046_is_touched(void) {
    // Check IRQ pin (active low when touched)
    return !gpio_get(TOUCH_PIN_IRQ);
}

bool xpt2046_read(touch_point_t *point) {
    if (!point) return false;
    
    // Check if touched
    if (!xpt2046_is_touched()) {
        point->is_pressed = false;
        return false;
    }
    
    // Read X, Y, and pressure
    uint16_t x = xpt2046_read_data(XPT2046_CMD_X);
    uint16_t y = xpt2046_read_data(XPT2046_CMD_Y);
    uint16_t z1 = xpt2046_read_data(XPT2046_CMD_Z1);
    uint16_t z2 = xpt2046_read_data(XPT2046_CMD_Z2);
    
    // Calculate pressure
    uint16_t pressure = 0;
    if (z1 != 0) {
        pressure = (x * (z2 - z1)) / z1;
    }
    
    // Check if pressure is sufficient
    if (pressure < PRESSURE_THRESHOLD) {
        point->is_pressed = false;
        return false;
    }
    
    // Fill in touch point
    point->x = x;
    point->y = y;
    point->pressure = pressure;
    point->is_pressed = true;
    
    return true;
}

void xpt2046_calibrate(uint16_t raw_x, uint16_t raw_y, 
                       uint16_t *screen_x, uint16_t *screen_y) {
    if (!screen_x || !screen_y) return;
    
    // Map raw coordinates to screen coordinates
    // Account for display rotation (landscape mode)
    int32_t x = ((int32_t)raw_x - TOUCH_X_MIN) * DISPLAY_WIDTH / (TOUCH_X_MAX - TOUCH_X_MIN);
    int32_t y = ((int32_t)raw_y - TOUCH_Y_MIN) * DISPLAY_HEIGHT / (TOUCH_Y_MAX - TOUCH_Y_MIN);
    
    // Clamp to screen bounds
    if (x < 0) x = 0;
    if (x >= DISPLAY_WIDTH) x = DISPLAY_WIDTH - 1;
    if (y < 0) y = 0;
    if (y >= DISPLAY_HEIGHT) y = DISPLAY_HEIGHT - 1;
    
    *screen_x = (uint16_t)x;
    *screen_y = (uint16_t)y;
}

touch_gesture_t xpt2046_detect_gesture(void) {
    touch_point_t point;
    bool currently_touched = xpt2046_read(&point);
    
    // Touch started
    if (currently_touched && !_gesture_state.is_touching) {
        _gesture_state.is_touching = true;
        _gesture_state.start_x = point.x;
        _gesture_state.start_y = point.y;
        _gesture_state.last_x = point.x;
        _gesture_state.last_y = point.y;
        _gesture_state.touch_start_time = get_absolute_time();
        return GESTURE_NONE;
    }
    
    // Touch continuing
    if (currently_touched && _gesture_state.is_touching) {
        _gesture_state.last_x = point.x;
        _gesture_state.last_y = point.y;
        return GESTURE_NONE;
    }
    
    // Touch ended
    if (!currently_touched && _gesture_state.is_touching) {
        _gesture_state.is_touching = false;
        _gesture_state.touch_end_time = get_absolute_time();
        
        // Calculate touch duration
        int64_t duration_ms = absolute_time_diff_us(_gesture_state.touch_start_time, 
                                                     _gesture_state.touch_end_time) / 1000;
        
        // Calculate movement
        int32_t dx = (int32_t)_gesture_state.last_x - (int32_t)_gesture_state.start_x;
        int32_t dy = (int32_t)_gesture_state.last_y - (int32_t)_gesture_state.start_y;
        int32_t distance = dx * dx + dy * dy;  // Squared distance
        
        // Long press detection
        if (duration_ms > TOUCH_HOLD_TIME_MS && distance < (SWIPE_THRESHOLD_PX * SWIPE_THRESHOLD_PX)) {
            return GESTURE_LONG_PRESS;
        }
        
        // Tap detection
        if (duration_ms < SWIPE_TIMEOUT_MS && distance < (SWIPE_THRESHOLD_PX * SWIPE_THRESHOLD_PX)) {
            return GESTURE_TAP;
        }
        
        // Swipe detection
        if (duration_ms < SWIPE_TIMEOUT_MS && distance >= (SWIPE_THRESHOLD_PX * SWIPE_THRESHOLD_PX)) {
            // Determine swipe direction
            if (abs(dx) > abs(dy)) {
                // Horizontal swipe
                return (dx > 0) ? GESTURE_SWIPE_RIGHT : GESTURE_SWIPE_LEFT;
            } else {
                // Vertical swipe
                return (dy > 0) ? GESTURE_SWIPE_DOWN : GESTURE_SWIPE_UP;
            }
        }
    }
    
    return GESTURE_NONE;
}

