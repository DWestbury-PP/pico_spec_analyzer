/**
 * @file display_test.c
 * @brief Simple ILI9341 display test
 * 
 * Tests the display wiring by:
 * - Initializing SPI and ILI9341
 * - Running color test patterns
 * - Drawing shapes
 */

#include <stdio.h>
#include "pico/stdlib.h"
#include "display/ili9341.h"
#include "config.h"

int main(void) {
    // Initialize stdio - wait for USB
    stdio_init_all();
    sleep_ms(2000);
    
    printf("\n\n");
    printf("====================================\n");
    printf("  ILI9341 Display Test\n");
    printf("====================================\n\n");
    
    printf("Display Configuration:\n");
    printf("  SPI Port: SPI0\n");
    printf("  Speed: %d MHz\n", DISPLAY_SPI_SPEED / 1000000);
    printf("  CS Pin: GP%d\n", DISPLAY_PIN_CS);
    printf("  DC Pin: GP%d\n", DISPLAY_PIN_DC);
    printf("  RST Pin: GP%d\n", DISPLAY_PIN_RST);
    printf("  SCK Pin: GP%d\n", DISPLAY_PIN_SCK);
    printf("  MOSI Pin: GP%d\n", DISPLAY_PIN_MOSI);
    printf("\n");
    
    printf("Initializing display...\n");
    if (!ili9341_init()) {
        printf("ERROR: Display initialization failed!\n");
        return 1;
    }
    printf("Display initialized successfully!\n\n");
    
    // Set rotation (1 = 90 degrees, landscape mode)
    printf("Setting rotation to %d (landscape)\n", DISPLAY_ROTATION);
    ili9341_set_rotation(DISPLAY_ROTATION);
    printf("Display size: %dx%d\n\n", ili9341_width(), ili9341_height());
    
    // Run test pattern
    printf("Running test pattern...\n");
    printf("(Watch the display for color changes)\n\n");
    ili9341_test_pattern();
    
    printf("\n====================================\n");
    printf("  Test Complete!\n");
    printf("====================================\n\n");
    
    printf("The display should show:\n");
    printf("  - Colored rectangles at top\n");
    printf("  - Outlined rectangles at bottom\n\n");
    
    printf("If you see this pattern, your display\n");
    printf("is wired correctly!\n\n");
    
    // Keep display showing test pattern
    printf("Test will run continuously...\n");
    printf("Press Ctrl+C to stop monitoring.\n\n");
    
    uint32_t count = 0;
    while (true) {
        // Blink a small indicator in corner every second
        if (count % 10 == 0) {
            ili9341_fill_rect(0, 0, 5, 5, ILI9341_WHITE);
            printf(".");
            if (count % 100 == 0) printf("\n");
        } else if (count % 10 == 5) {
            ili9341_fill_rect(0, 0, 5, 5, ILI9341_BLACK);
        }
        
        sleep_ms(100);
        count++;
    }
    
    return 0;
}

