/**
 * @file main_simple_test.c
 * @brief Ultra-simple LED blink test
 * 
 * This is a minimal test to verify:
 * 1. The build toolchain works
 * 2. The Pico can be flashed
 * 3. GPIO control works
 * 4. USB serial works
 */

#include <stdio.h>
#include "pico/stdlib.h"

#define LED_PIN 15

int main(void) {
    // Initialize stdio - wait for USB to enumerate
    stdio_init_all();
    
    // Give USB time to initialize (important!)
    sleep_ms(2000);
    
    // Print repeatedly to help catch output
    for (int i = 0; i < 10; i++) {
        printf("=== HELLO FROM PICO ===\n");
        sleep_ms(100);
    }
    
    printf("Initializing GPIO %d for LED...\n", LED_PIN);
    
    // Initialize LED pin
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    
    printf("Starting blink loop...\n");
    printf("LED should blink every 500ms\n");
    
    uint32_t count = 0;
    
    // Infinite blink loop
    while (true) {
        gpio_put(LED_PIN, 1);
        printf("LED ON (count=%lu)\n", count);
        sleep_ms(500);
        
        gpio_put(LED_PIN, 0);
        printf("LED OFF\n");
        sleep_ms(500);
        
        count++;
    }
    
    return 0;
}

