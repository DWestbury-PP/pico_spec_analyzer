/**
 * @file adc_sampler.c
 * @brief ADC audio sampling implementation
 * 
 * Uses free-running ADC with DMA for efficient sample capture.
 * Timer triggers ADC conversions at precise intervals.
 */

#include "audio/adc_sampler.h"
#include "hardware/adc.h"
#include "hardware/dma.h"
#include "hardware/timer.h"
#include "hardware/irq.h"
#include "config.h"
#include <string.h>

// ============================================================================
// Configuration
// ============================================================================

#define BUFFER_SIZE 1024  // Must be power of 2
#define BUFFER_MASK (BUFFER_SIZE - 1)

// ============================================================================
// Private State
// ============================================================================

static uint16_t _sample_buffer[BUFFER_SIZE];
static volatile uint32_t _write_pos = 0;
static volatile uint32_t _read_pos = 0;
static uint32_t _sample_rate_hz = 0;
static uint8_t _adc_channel = 0;
static int _dma_channel = -1;
static struct repeating_timer _sample_timer;
static volatile bool _is_running = false;

// ============================================================================
// Private Functions
// ============================================================================

/**
 * @brief Timer callback to trigger ADC conversion
 */
static bool sample_timer_callback(struct repeating_timer *t) {
    if (!_is_running) return true;
    
    // Trigger ADC conversion
    adc_select_input(_adc_channel);
    adc_run(true);
    
    // Read result (12-bit ADC)
    uint16_t sample = adc_fifo_get_blocking();
    
    // Store in circular buffer
    _sample_buffer[_write_pos] = sample;
    _write_pos = (_write_pos + 1) & BUFFER_MASK;
    
    // Check for buffer overflow
    if (_write_pos == _read_pos) {
        // Buffer full, drop oldest sample
        _read_pos = (_read_pos + 1) & BUFFER_MASK;
    }
    
    return true;  // Continue timer
}

// ============================================================================
// Public API
// ============================================================================

bool adc_sampler_init(uint8_t adc_channel, uint32_t sample_rate_hz) {
    if (adc_channel > 3) return false;
    if (sample_rate_hz == 0 || sample_rate_hz > 500000) return false;
    
    _adc_channel = adc_channel;
    _sample_rate_hz = sample_rate_hz;
    
    // Initialize ADC
    adc_init();
    
    // Configure ADC GPIO pin
    uint8_t adc_pin = 26 + adc_channel;
    adc_gpio_init(adc_pin);
    
    // Select ADC input
    adc_select_input(adc_channel);
    
    // Set up ADC FIFO
    adc_fifo_setup(
        true,    // Enable FIFO
        false,   // No DMA
        1,       // Trigger threshold
        false,   // No error bit
        false    // No byte shifting
    );
    
    // Clear buffer
    memset(_sample_buffer, 0, sizeof(_sample_buffer));
    _write_pos = 0;
    _read_pos = 0;
    _is_running = false;
    
    DEBUG_PRINTF("ADC sampler initialized: CH%d @ %lu Hz\n", adc_channel, sample_rate_hz);
    
    return true;
}

void adc_sampler_start(void) {
    if (_is_running) return;
    
    _is_running = true;
    
    // Calculate timer interval in microseconds
    int64_t interval_us = 1000000LL / _sample_rate_hz;
    
    // Start repeating timer
    add_repeating_timer_us(-interval_us, sample_timer_callback, NULL, &_sample_timer);
    
    DEBUG_PRINTF("ADC sampler started\n");
}

void adc_sampler_stop(void) {
    if (!_is_running) return;
    
    _is_running = false;
    cancel_repeating_timer(&_sample_timer);
    adc_run(false);
    
    DEBUG_PRINTF("ADC sampler stopped\n");
}

uint32_t adc_sampler_available(void) {
    uint32_t write = _write_pos;
    uint32_t read = _read_pos;
    
    if (write >= read) {
        return write - read;
    } else {
        return BUFFER_SIZE - read + write;
    }
}

uint32_t adc_sampler_read(uint16_t *buffer, uint32_t count) {
    if (!buffer || count == 0) return 0;
    
    uint32_t available = adc_sampler_available();
    uint32_t to_read = (count < available) ? count : available;
    
    for (uint32_t i = 0; i < to_read; i++) {
        buffer[i] = _sample_buffer[_read_pos];
        _read_pos = (_read_pos + 1) & BUFFER_MASK;
    }
    
    return to_read;
}

uint32_t adc_sampler_get_rate(void) {
    return _sample_rate_hz;
}

