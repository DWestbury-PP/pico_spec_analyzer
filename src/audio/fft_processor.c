/**
 * @file fft_processor.c
 * @brief FFT processing implementation
 * 
 * Uses a simple real FFT implementation optimized for Cortex-M0+.
 * Extracts frequency bands from FFT output for visualization.
 */

#include "audio/fft_processor.h"
#include "config.h"
#include <math.h>
#include <string.h>

// ============================================================================
// Configuration
// ============================================================================

#define PI 3.14159265358979323846f
#define TWO_PI (2.0f * PI)

// ============================================================================
// Private State
// ============================================================================

static uint32_t _sample_rate_hz = 0;
static float _fft_input[FFT_SIZE];
static float _fft_output[FFT_SIZE];
static float _window[FFT_SIZE];

// ============================================================================
// Simple FFT Implementation
// ============================================================================

/**
 * @brief Bit-reverse permutation for FFT
 */
static uint32_t bit_reverse(uint32_t x, uint32_t bits) {
    uint32_t result = 0;
    for (uint32_t i = 0; i < bits; i++) {
        result = (result << 1) | (x & 1);
        x >>= 1;
    }
    return result;
}

/**
 * @brief Simple radix-2 FFT implementation
 * 
 * This is a basic FFT for educational purposes and initial testing.
 * For production, consider using CMSIS-DSP or pico-extras FFT.
 */
static void simple_fft(float *real, float *imag, uint32_t n) {
    // Bit-reverse permutation
    uint32_t bits = 0;
    uint32_t temp = n;
    while (temp > 1) {
        bits++;
        temp >>= 1;
    }
    
    for (uint32_t i = 0; i < n; i++) {
        uint32_t j = bit_reverse(i, bits);
        if (j > i) {
            // Swap real parts
            float temp_r = real[i];
            real[i] = real[j];
            real[j] = temp_r;
            
            // Swap imaginary parts
            float temp_i = imag[i];
            imag[i] = imag[j];
            imag[j] = temp_i;
        }
    }
    
    // FFT butterfly operations
    for (uint32_t size = 2; size <= n; size *= 2) {
        float angle = -TWO_PI / size;
        float wlen_r = cosf(angle);
        float wlen_i = sinf(angle);
        
        for (uint32_t i = 0; i < n; i += size) {
            float w_r = 1.0f;
            float w_i = 0.0f;
            
            for (uint32_t j = 0; j < size / 2; j++) {
                uint32_t idx1 = i + j;
                uint32_t idx2 = i + j + size / 2;
                
                float u_r = real[idx1];
                float u_i = imag[idx1];
                float v_r = real[idx2] * w_r - imag[idx2] * w_i;
                float v_i = real[idx2] * w_i + imag[idx2] * w_r;
                
                real[idx1] = u_r + v_r;
                imag[idx1] = u_i + v_i;
                real[idx2] = u_r - v_r;
                imag[idx2] = u_i - v_i;
                
                // Update twiddle factor
                float w_r_temp = w_r * wlen_r - w_i * wlen_i;
                w_i = w_r * wlen_i + w_i * wlen_r;
                w_r = w_r_temp;
            }
        }
    }
}

// ============================================================================
// Public API
// ============================================================================

bool fft_processor_init(uint32_t sample_rate_hz) {
    if (sample_rate_hz == 0) return false;
    
    _sample_rate_hz = sample_rate_hz;
    
    // Generate Hann window
    for (uint32_t i = 0; i < FFT_SIZE; i++) {
        _window[i] = 0.5f * (1.0f - cosf(TWO_PI * i / (FFT_SIZE - 1)));
    }
    
    DEBUG_PRINTF("FFT processor initialized: %lu Hz, size %d\n", sample_rate_hz, FFT_SIZE);
    
    return true;
}

bool fft_processor_compute(const uint16_t *samples, float *bands, uint8_t num_bands) {
    if (!samples || !bands || num_bands == 0) return false;
    
    // Convert samples to float and apply window
    // ADC gives 12-bit values (0-4095), centered around 2048
    for (uint32_t i = 0; i < FFT_SIZE; i++) {
        // Remove DC offset and normalize to -1.0 to 1.0
        float sample = ((float)samples[i] - 2048.0f) / 2048.0f;
        _fft_input[i] = sample * _window[i];
    }
    
    // Initialize imaginary part to zero
    memset(_fft_output, 0, sizeof(_fft_output));
    
    // Perform FFT
    simple_fft(_fft_input, _fft_output, FFT_SIZE);
    
    // Calculate magnitude spectrum (only first half, due to symmetry)
    float magnitudes[FFT_SIZE / 2];
    for (uint32_t i = 0; i < FFT_SIZE / 2; i++) {
        float real = _fft_input[i];
        float imag = _fft_output[i];
        magnitudes[i] = sqrtf(real * real + imag * imag);
    }
    
    // Extract frequency bands
    // Use logarithmic spacing for more natural frequency distribution
    float freq_min = FREQ_MIN_HZ;
    float freq_max = FREQ_MAX_HZ;
    float log_min = logf(freq_min);
    float log_max = logf(freq_max);
    
    for (uint8_t band = 0; band < num_bands; band++) {
        // Calculate frequency range for this band (logarithmic)
        float t0 = (float)band / num_bands;
        float t1 = (float)(band + 1) / num_bands;
        float f0 = expf(log_min + t0 * (log_max - log_min));
        float f1 = expf(log_min + t1 * (log_max - log_min));
        
        // Convert frequencies to FFT bin indices
        uint32_t bin0 = (uint32_t)(f0 * FFT_SIZE / _sample_rate_hz);
        uint32_t bin1 = (uint32_t)(f1 * FFT_SIZE / _sample_rate_hz);
        
        // Clamp to valid range
        if (bin0 >= FFT_SIZE / 2) bin0 = FFT_SIZE / 2 - 1;
        if (bin1 >= FFT_SIZE / 2) bin1 = FFT_SIZE / 2 - 1;
        if (bin1 <= bin0) bin1 = bin0 + 1;
        
        // Average magnitude across bins in this band
        float sum = 0.0f;
        uint32_t count = 0;
        for (uint32_t bin = bin0; bin < bin1 && bin < FFT_SIZE / 2; bin++) {
            sum += magnitudes[bin];
            count++;
        }
        
        float avg = (count > 0) ? (sum / count) : 0.0f;
        
        // Normalize and apply logarithmic scaling for better visualization
        // Use configurable gain for display sensitivity
        avg = avg / FFT_DISPLAY_GAIN;  // Adjust in config.h if needed
        
        // Apply logarithmic compression
        if (avg > 0.0f) {
            avg = logf(1.0f + avg * 10.0f) / logf(11.0f);
        }
        
        // Clamp to 0-1 range
        if (avg < 0.0f) avg = 0.0f;
        if (avg > 1.0f) avg = 1.0f;
        
        bands[band] = avg;
    }
    
    return true;
}

void fft_processor_get_band_range(uint8_t band_index, uint8_t num_bands, 
                                   float *freq_min, float *freq_max) {
    if (!freq_min || !freq_max || num_bands == 0) return;
    
    float log_min = logf(FREQ_MIN_HZ);
    float log_max = logf(FREQ_MAX_HZ);
    
    float t0 = (float)band_index / num_bands;
    float t1 = (float)(band_index + 1) / num_bands;
    
    *freq_min = expf(log_min + t0 * (log_max - log_min));
    *freq_max = expf(log_min + t1 * (log_max - log_min));
}

