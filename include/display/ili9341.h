/**
 * @file ili9341.h
 * @brief ILI9341 TFT Display Driver for Raspberry Pi Pico
 * 
 * Driver for 320x240 16-bit color TFT displays using ILI9341 controller
 */

#ifndef ILI9341_H
#define ILI9341_H

#include <stdint.h>
#include <stdbool.h>
#include "pico/stdlib.h"

// ============================================================================
// ILI9341 Commands
// ============================================================================

#define ILI9341_NOP         0x00
#define ILI9341_SWRESET     0x01
#define ILI9341_RDDID       0x04
#define ILI9341_RDDST       0x09

#define ILI9341_SLPIN       0x10
#define ILI9341_SLPOUT      0x11
#define ILI9341_PTLON       0x12
#define ILI9341_NORON       0x13

#define ILI9341_INVOFF      0x20
#define ILI9341_INVON       0x21
#define ILI9341_GAMMASET    0x26
#define ILI9341_DISPOFF     0x28
#define ILI9341_DISPON      0x29

#define ILI9341_CASET       0x2A
#define ILI9341_PASET       0x2B
#define ILI9341_RAMWR       0x2C
#define ILI9341_RAMRD       0x2E

#define ILI9341_PTLAR       0x30
#define ILI9341_MADCTL      0x36
#define ILI9341_PIXFMT      0x3A

#define ILI9341_FRMCTR1     0xB1
#define ILI9341_FRMCTR2     0xB2
#define ILI9341_FRMCTR3     0xB3
#define ILI9341_INVCTR      0xB4
#define ILI9341_DFUNCTR     0xB6

#define ILI9341_PWCTR1      0xC0
#define ILI9341_PWCTR2      0xC1
#define ILI9341_PWCTR3      0xC2
#define ILI9341_PWCTR4      0xC3
#define ILI9341_PWCTR5      0xC4
#define ILI9341_VMCTR1      0xC5
#define ILI9341_VMCTR2      0xC7

#define ILI9341_GMCTRP1     0xE0
#define ILI9341_GMCTRN1     0xE1

// ============================================================================
// Color Definitions (RGB565 format)
// ============================================================================

#define ILI9341_BLACK       0x0000
#define ILI9341_NAVY        0x000F
#define ILI9341_DARKGREEN   0x03E0
#define ILI9341_DARKCYAN    0x03EF
#define ILI9341_MAROON      0x7800
#define ILI9341_PURPLE      0x780F
#define ILI9341_OLIVE       0x7BE0
#define ILI9341_LIGHTGREY   0xC618
#define ILI9341_DARKGREY    0x7BEF
#define ILI9341_BLUE        0x001F
#define ILI9341_GREEN       0x07E0
#define ILI9341_CYAN        0x07FF
#define ILI9341_RED         0xF800
#define ILI9341_MAGENTA     0xF81F
#define ILI9341_YELLOW      0xFFE0
#define ILI9341_WHITE       0xFFFF
#define ILI9341_ORANGE      0xFD20
#define ILI9341_GREENYELLOW 0xAFE5
#define ILI9341_PINK        0xF81F

// ============================================================================
// Display Configuration
// ============================================================================

#define ILI9341_TFTWIDTH    240
#define ILI9341_TFTHEIGHT   320

// Rotation values
#define ILI9341_ROTATION_0   0
#define ILI9341_ROTATION_90  1
#define ILI9341_ROTATION_180 2
#define ILI9341_ROTATION_270 3

// ============================================================================
// Public API
// ============================================================================

/**
 * @brief Initialize ILI9341 display
 * @return true if successful, false on error
 */
bool ili9341_init(void);

/**
 * @brief Set display rotation
 * @param rotation Rotation value (0-3)
 */
void ili9341_set_rotation(uint8_t rotation);

/**
 * @brief Fill entire screen with single color
 * @param color RGB565 color value
 */
void ili9341_fill_screen(uint16_t color);

/**
 * @brief Draw a single pixel
 * @param x X coordinate
 * @param y Y coordinate
 * @param color RGB565 color value
 */
void ili9341_draw_pixel(int16_t x, int16_t y, uint16_t color);

/**
 * @brief Draw a filled rectangle
 * @param x X coordinate of top-left corner
 * @param y Y coordinate of top-left corner
 * @param w Width
 * @param h Height
 * @param color RGB565 color value
 */
void ili9341_fill_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);

/**
 * @brief Draw a rectangle outline
 * @param x X coordinate of top-left corner
 * @param y Y coordinate of top-left corner
 * @param w Width
 * @param h Height
 * @param color RGB565 color value
 */
void ili9341_draw_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);

/**
 * @brief Set address window for bulk pixel writing
 * @param x0 Start X coordinate
 * @param y0 Start Y coordinate
 * @param x1 End X coordinate
 * @param y1 End Y coordinate
 */
void ili9341_set_addr_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);

/**
 * @brief Begin writing pixel data (call after set_addr_window)
 */
void ili9341_begin_write(void);

/**
 * @brief Write a single pixel value (16-bit color)
 * @param color RGB565 color value
 */
void ili9341_write_pixel(uint16_t color);

/**
 * @brief End pixel data write
 */
void ili9341_end_write(void);

/**
 * @brief Run display test pattern
 */
void ili9341_test_pattern(void);

/**
 * @brief Get current display width (accounts for rotation)
 * @return Width in pixels
 */
uint16_t ili9341_width(void);

/**
 * @brief Get current display height (accounts for rotation)
 * @return Height in pixels
 */
uint16_t ili9341_height(void);

#endif // ILI9341_H

