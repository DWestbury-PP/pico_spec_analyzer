/**
 * @file ili9341.c
 * @brief ILI9341 TFT Display Driver Implementation
 */

#include "display/ili9341.h"
#include "config.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include <string.h>

// ============================================================================
// Private State
// ============================================================================

static uint16_t _width = ILI9341_TFTWIDTH;
static uint16_t _height = ILI9341_TFTHEIGHT;
static uint8_t _rotation = 0;

// ============================================================================
// Low-Level SPI Communication
// ============================================================================

/**
 * @brief Write command byte to display
 */
static inline void write_command(uint8_t cmd) {
    gpio_put(DISPLAY_PIN_DC, 0);  // Command mode
    gpio_put(DISPLAY_PIN_CS, 0);  // Select display
    spi_write_blocking(DISPLAY_SPI_PORT, &cmd, 1);
    gpio_put(DISPLAY_PIN_CS, 1);  // Deselect
}

/**
 * @brief Write single data byte to display
 */
static inline void write_data(uint8_t data) {
    gpio_put(DISPLAY_PIN_DC, 1);  // Data mode
    gpio_put(DISPLAY_PIN_CS, 0);  // Select display
    spi_write_blocking(DISPLAY_SPI_PORT, &data, 1);
    gpio_put(DISPLAY_PIN_CS, 1);  // Deselect
}

/**
 * @brief Write multiple data bytes to display
 */
static inline void write_data_buf(const uint8_t *buf, size_t len) {
    gpio_put(DISPLAY_PIN_DC, 1);  // Data mode
    gpio_put(DISPLAY_PIN_CS, 0);  // Select display
    spi_write_blocking(DISPLAY_SPI_PORT, buf, len);
    gpio_put(DISPLAY_PIN_CS, 1);  // Deselect
}

/**
 * @brief Write command with data bytes
 */
static void write_command_data(uint8_t cmd, const uint8_t *data, size_t len) {
    write_command(cmd);
    if (len > 0) {
        write_data_buf(data, len);
    }
}

// ============================================================================
// Initialization
// ============================================================================

bool ili9341_init(void) {
    // Initialize SPI
    spi_init(DISPLAY_SPI_PORT, DISPLAY_SPI_SPEED);
    
    // Set SPI format: 8 bits, SPI mode 0
    spi_set_format(DISPLAY_SPI_PORT, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    
    // Initialize GPIO pins
    gpio_set_function(DISPLAY_PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(DISPLAY_PIN_MOSI, GPIO_FUNC_SPI);
    
    // CS, DC, and RST are regular GPIO outputs
    gpio_init(DISPLAY_PIN_CS);
    gpio_set_dir(DISPLAY_PIN_CS, GPIO_OUT);
    gpio_put(DISPLAY_PIN_CS, 1);  // Deselect
    
    gpio_init(DISPLAY_PIN_DC);
    gpio_set_dir(DISPLAY_PIN_DC, GPIO_OUT);
    gpio_put(DISPLAY_PIN_DC, 1);
    
    gpio_init(DISPLAY_PIN_RST);
    gpio_set_dir(DISPLAY_PIN_RST, GPIO_OUT);
    gpio_put(DISPLAY_PIN_RST, 1);
    
    // Optional: Backlight control
    #ifdef DISPLAY_PIN_BL
    gpio_init(DISPLAY_PIN_BL);
    gpio_set_dir(DISPLAY_PIN_BL, GPIO_OUT);
    gpio_put(DISPLAY_PIN_BL, 1);  // Turn on backlight
    #endif
    
    // Hardware reset
    gpio_put(DISPLAY_PIN_RST, 1);
    sleep_ms(5);
    gpio_put(DISPLAY_PIN_RST, 0);
    sleep_ms(20);
    gpio_put(DISPLAY_PIN_RST, 1);
    sleep_ms(150);
    
    // Software reset
    write_command(ILI9341_SWRESET);
    sleep_ms(150);
    
    // Power control A
    write_command_data(0xCB, (const uint8_t[]){0x39, 0x2C, 0x00, 0x34, 0x02}, 5);
    
    // Power control B
    write_command_data(0xCF, (const uint8_t[]){0x00, 0xC1, 0x30}, 3);
    
    // Driver timing control A
    write_command_data(0xE8, (const uint8_t[]){0x85, 0x00, 0x78}, 3);
    
    // Driver timing control B
    write_command_data(0xEA, (const uint8_t[]){0x00, 0x00}, 2);
    
    // Power on sequence control
    write_command_data(0xED, (const uint8_t[]){0x64, 0x03, 0x12, 0x81}, 4);
    
    // Pump ratio control
    write_command_data(0xF7, (const uint8_t[]){0x20}, 1);
    
    // Power control 1
    write_command_data(ILI9341_PWCTR1, (const uint8_t[]){0x23}, 1);
    
    // Power control 2
    write_command_data(ILI9341_PWCTR2, (const uint8_t[]){0x10}, 1);
    
    // VCOM control 1
    write_command_data(ILI9341_VMCTR1, (const uint8_t[]){0x3e, 0x28}, 2);
    
    // VCOM control 2
    write_command_data(ILI9341_VMCTR2, (const uint8_t[]){0x86}, 1);
    
    // Memory access control (rotation)
    write_command_data(ILI9341_MADCTL, (const uint8_t[]){0x48}, 1);
    
    // Pixel format: 16-bit color
    write_command_data(ILI9341_PIXFMT, (const uint8_t[]){0x55}, 1);
    
    // Frame rate control
    write_command_data(ILI9341_FRMCTR1, (const uint8_t[]){0x00, 0x18}, 2);
    
    // Display function control
    write_command_data(ILI9341_DFUNCTR, (const uint8_t[]){0x08, 0x82, 0x27}, 3);
    
    // Enable 3 gamma control
    write_command_data(0xF2, (const uint8_t[]){0x00}, 1);
    
    // Gamma curve
    write_command_data(ILI9341_GAMMASET, (const uint8_t[]){0x01}, 1);
    
    // Positive gamma correction
    write_command_data(ILI9341_GMCTRP1, (const uint8_t[]){
        0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08, 0x4E, 0xF1,
        0x37, 0x07, 0x10, 0x03, 0x0E, 0x09, 0x00
    }, 15);
    
    // Negative gamma correction
    write_command_data(ILI9341_GMCTRN1, (const uint8_t[]){
        0x00, 0x0E, 0x14, 0x03, 0x11, 0x07, 0x31, 0xC1,
        0x48, 0x08, 0x0F, 0x0C, 0x31, 0x36, 0x0F
    }, 15);
    
    // Sleep out
    write_command(ILI9341_SLPOUT);
    sleep_ms(120);
    
    // Display on
    write_command(ILI9341_DISPON);
    sleep_ms(20);
    
    DEBUG_PRINTF("ILI9341 initialized\n");
    return true;
}

// ============================================================================
// Configuration
// ============================================================================

void ili9341_set_rotation(uint8_t rotation) {
    _rotation = rotation % 4;
    
    uint8_t madctl = 0;
    
    switch (_rotation) {
        case ILI9341_ROTATION_0:
            madctl = 0x48;  // MX, BGR
            _width = ILI9341_TFTWIDTH;
            _height = ILI9341_TFTHEIGHT;
            break;
        case ILI9341_ROTATION_90:
            madctl = 0x28;  // MV, BGR
            _width = ILI9341_TFTHEIGHT;
            _height = ILI9341_TFTWIDTH;
            break;
        case ILI9341_ROTATION_180:
            madctl = 0x88;  // MY, BGR
            _width = ILI9341_TFTWIDTH;
            _height = ILI9341_TFTHEIGHT;
            break;
        case ILI9341_ROTATION_270:
            madctl = 0xE8;  // MX, MY, MV, BGR
            _width = ILI9341_TFTHEIGHT;
            _height = ILI9341_TFTWIDTH;
            break;
    }
    
    write_command_data(ILI9341_MADCTL, &madctl, 1);
}

// ============================================================================
// Drawing Functions
// ============================================================================

void ili9341_set_addr_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    write_command(ILI9341_CASET);  // Column address set
    write_data(x0 >> 8);
    write_data(x0 & 0xFF);
    write_data(x1 >> 8);
    write_data(x1 & 0xFF);
    
    write_command(ILI9341_PASET);  // Page address set
    write_data(y0 >> 8);
    write_data(y0 & 0xFF);
    write_data(y1 >> 8);
    write_data(y1 & 0xFF);
    
    write_command(ILI9341_RAMWR);  // Memory write
}

void ili9341_begin_write(void) {
    gpio_put(DISPLAY_PIN_DC, 1);  // Data mode
    gpio_put(DISPLAY_PIN_CS, 0);  // Select display
}

void ili9341_write_pixel(uint16_t color) {
    uint8_t buf[2] = {color >> 8, color & 0xFF};
    spi_write_blocking(DISPLAY_SPI_PORT, buf, 2);
}

void ili9341_end_write(void) {
    gpio_put(DISPLAY_PIN_CS, 1);  // Deselect display
}

void ili9341_fill_screen(uint16_t color) {
    ili9341_fill_rect(0, 0, _width, _height, color);
}

void ili9341_draw_pixel(int16_t x, int16_t y, uint16_t color) {
    if ((x < 0) || (x >= _width) || (y < 0) || (y >= _height)) return;
    
    ili9341_set_addr_window(x, y, x, y);
    ili9341_begin_write();
    ili9341_write_pixel(color);
    ili9341_end_write();
}

void ili9341_fill_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    if ((x >= _width) || (y >= _height)) return;
    if ((x + w - 1) >= _width) w = _width - x;
    if ((y + h - 1) >= _height) h = _height - y;
    
    ili9341_set_addr_window(x, y, x + w - 1, y + h - 1);
    
    uint8_t hi = color >> 8;
    uint8_t lo = color & 0xFF;
    
    ili9341_begin_write();
    for (int32_t i = (int32_t)w * h; i > 0; i--) {
        uint8_t buf[2] = {hi, lo};
        spi_write_blocking(DISPLAY_SPI_PORT, buf, 2);
    }
    ili9341_end_write();
}

void ili9341_draw_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    // Draw four lines to make a rectangle
    // Top
    ili9341_fill_rect(x, y, w, 1, color);
    // Bottom
    ili9341_fill_rect(x, y + h - 1, w, 1, color);
    // Left
    ili9341_fill_rect(x, y, 1, h, color);
    // Right
    ili9341_fill_rect(x + w - 1, y, 1, h, color);
}

// ============================================================================
// Test Pattern
// ============================================================================

void ili9341_test_pattern(void) {
    DEBUG_PRINTF("Running display test pattern...\n");
    
    // Fill screen with different colors
    DEBUG_PRINTF("  Red...\n");
    ili9341_fill_screen(ILI9341_RED);
    sleep_ms(500);
    
    DEBUG_PRINTF("  Green...\n");
    ili9341_fill_screen(ILI9341_GREEN);
    sleep_ms(500);
    
    DEBUG_PRINTF("  Blue...\n");
    ili9341_fill_screen(ILI9341_BLUE);
    sleep_ms(500);
    
    DEBUG_PRINTF("  White...\n");
    ili9341_fill_screen(ILI9341_WHITE);
    sleep_ms(500);
    
    DEBUG_PRINTF("  Black...\n");
    ili9341_fill_screen(ILI9341_BLACK);
    sleep_ms(500);
    
    // Draw colored rectangles
    DEBUG_PRINTF("  Color bars...\n");
    uint16_t colors[] = {
        ILI9341_RED, ILI9341_YELLOW, ILI9341_GREEN, ILI9341_CYAN,
        ILI9341_BLUE, ILI9341_MAGENTA, ILI9341_WHITE, ILI9341_BLACK
    };
    
    int bar_width = _width / 8;
    for (int i = 0; i < 8; i++) {
        ili9341_fill_rect(i * bar_width, 0, bar_width, _height, colors[i]);
    }
    sleep_ms(2000);
    
    // Clear to black
    ili9341_fill_screen(ILI9341_BLACK);
    
    // Draw some test rectangles
    DEBUG_PRINTF("  Test rectangles...\n");
    ili9341_fill_rect(10, 10, 60, 40, ILI9341_RED);
    ili9341_fill_rect(80, 20, 60, 40, ILI9341_GREEN);
    ili9341_fill_rect(150, 30, 60, 40, ILI9341_BLUE);
    
    ili9341_draw_rect(10, 100, 100, 80, ILI9341_YELLOW);
    ili9341_draw_rect(120, 110, 100, 80, ILI9341_CYAN);
    ili9341_draw_rect(230, 120, 80, 80, ILI9341_MAGENTA);
    
    DEBUG_PRINTF("Test pattern complete!\n");
}

// ============================================================================
// Getters
// ============================================================================

uint16_t ili9341_width(void) {
    return _width;
}

uint16_t ili9341_height(void) {
    return _height;
}

