/**
 * @file mock_audio.c
 * @brief Mock audio/FFT data generator for testing visualizations
 * 
 * Generates realistic-looking spectrum data with various patterns:
 * - Bass beats
 * - Frequency sweeps
 * - Random noise
 * - Sine wave tones
 */

#include "utils/mock_audio.h"
#include <stdlib.h>
#include <math.h>

// ============================================================================
// Private State
// ============================================================================

static uint32_t _frame_count = 0;
static mock_audio_pattern_t _current_pattern = PATTERN_MUSIC;

// ============================================================================
// Pattern Generators
// ============================================================================

/**
 * @brief Generate music-like spectrum with bass emphasis
 */
static void generate_music_pattern(float *bands, uint8_t num_bands) {
    // Simulate music with more energy in low frequencies
    float time = (float)_frame_count / 30.0f;  // Assuming 30 FPS
    
    for (int i = 0; i < num_bands; i++) {
        float freq_factor = (float)i / num_bands;
        
        // Bass beat (lower frequencies)
        float bass = sinf(time * 2.0f) * 0.5f + 0.5f;
        bass *= (1.0f - freq_factor);  // More energy in lower bands
        
        // Mid-range variation
        float mid = sinf(time * 3.0f + i * 0.5f) * 0.3f + 0.3f;
        mid *= (freq_factor > 0.3f && freq_factor < 0.7f) ? 1.0f : 0.3f;
        
        // High frequency sparkle
        float high = ((float)rand() / RAND_MAX) * 0.2f;
        high *= (freq_factor > 0.7f) ? 1.0f : 0.1f;
        
        // Combine
        bands[i] = bass + mid + high;
        
        // Add some random variation
        bands[i] += ((float)rand() / RAND_MAX - 0.5f) * 0.1f;
        
        // Clamp to 0-1 range
        if (bands[i] < 0.0f) bands[i] = 0.0f;
        if (bands[i] > 1.0f) bands[i] = 1.0f;
    }
}

/**
 * @brief Generate frequency sweep pattern
 */
static void generate_sweep_pattern(float *bands, uint8_t num_bands) {
    float time = (float)_frame_count / 60.0f;
    
    // Sweep across frequency bands
    int peak_band = (int)(sinf(time * 2.0f) * 0.5f + 0.5f) * (num_bands - 1);
    
    for (int i = 0; i < num_bands; i++) {
        // Gaussian peak around the current band
        float distance = fabsf((float)i - (float)peak_band);
        bands[i] = expf(-(distance * distance) / 8.0f) * 0.9f;
        
        // Add slight noise floor
        bands[i] += 0.05f;
    }
}

/**
 * @brief Generate random noise pattern
 */
static void generate_noise_pattern(float *bands, uint8_t num_bands) {
    for (int i = 0; i < num_bands; i++) {
        bands[i] = (float)rand() / RAND_MAX * 0.7f + 0.1f;
    }
}

/**
 * @brief Generate bass-heavy pattern (like kick drum)
 */
static void generate_bass_pattern(float *bands, uint8_t num_bands) {
    float time = (float)_frame_count / 30.0f;
    
    // Strong bass beat at ~2 Hz
    float beat = sinf(time * 4.0f * M_PI);
    beat = beat * beat * beat * beat;  // Sharpen the pulse
    beat = beat * 0.5f + 0.5f;
    
    for (int i = 0; i < num_bands; i++) {
        float freq_factor = (float)i / num_bands;
        
        // Exponential decay from bass to treble
        float decay = expf(-freq_factor * 5.0f);
        
        bands[i] = beat * decay;
        
        // Add harmonics
        if (i < num_bands / 2) {
            float harmonic = sinf(time * 8.0f * M_PI + i) * 0.2f;
            bands[i] += harmonic * (1.0f - freq_factor);
        }
        
        // Clamp
        if (bands[i] < 0.0f) bands[i] = 0.0f;
        if (bands[i] > 1.0f) bands[i] = 1.0f;
    }
}

/**
 * @brief Generate single tone at specific frequency
 */
static void generate_tone_pattern(float *bands, uint8_t num_bands) {
    float time = (float)_frame_count / 30.0f;
    
    // Slowly moving tone
    int tone_band = ((int)(time * 2.0f) % num_bands);
    
    for (int i = 0; i < num_bands; i++) {
        if (i == tone_band) {
            bands[i] = 0.8f;
        } else {
            // Slight bleed to adjacent bands
            int distance = abs(i - tone_band);
            if (distance == 1) {
                bands[i] = 0.2f;
            } else {
                bands[i] = 0.05f;
            }
        }
    }
}

/**
 * @brief Generate VU meter style (average level)
 */
static void generate_vu_pattern(float *bands, uint8_t num_bands) {
    float time = (float)_frame_count / 30.0f;
    
    // Smooth level variation
    float level = sinf(time * 1.5f) * 0.4f + 0.5f;
    
    // All bands show the same level (like VU meter)
    for (int i = 0; i < num_bands; i++) {
        bands[i] = level;
        
        // Slight variation per band
        bands[i] += sinf(time * 3.0f + i * 0.1f) * 0.1f;
        
        // Clamp
        if (bands[i] < 0.0f) bands[i] = 0.0f;
        if (bands[i] > 1.0f) bands[i] = 1.0f;
    }
}

// ============================================================================
// Public API
// ============================================================================

void mock_audio_init(void) {
    _frame_count = 0;
    _current_pattern = PATTERN_MUSIC;
    
    // Seed random number generator
    srand(12345);  // Fixed seed for reproducibility
}

void mock_audio_generate(float *bands, uint8_t num_bands, mock_audio_pattern_t pattern) {
    if (!bands || num_bands == 0) return;
    
    // Override pattern if specified, otherwise use current
    if (pattern != PATTERN_AUTO) {
        _current_pattern = pattern;
    }
    
    // Cycle through patterns in auto mode
    if (pattern == PATTERN_AUTO) {
        // Change pattern every 5 seconds (150 frames at 30 FPS)
        uint32_t pattern_index = (_frame_count / 150) % 6;
        _current_pattern = (mock_audio_pattern_t)pattern_index;
    }
    
    // Generate pattern
    switch (_current_pattern) {
        case PATTERN_MUSIC:
            generate_music_pattern(bands, num_bands);
            break;
        case PATTERN_SWEEP:
            generate_sweep_pattern(bands, num_bands);
            break;
        case PATTERN_NOISE:
            generate_noise_pattern(bands, num_bands);
            break;
        case PATTERN_BASS:
            generate_bass_pattern(bands, num_bands);
            break;
        case PATTERN_TONE:
            generate_tone_pattern(bands, num_bands);
            break;
        case PATTERN_VU:
            generate_vu_pattern(bands, num_bands);
            break;
        default:
            // Fill with zeros
            for (int i = 0; i < num_bands; i++) {
                bands[i] = 0.0f;
            }
            break;
    }
    
    _frame_count++;
}

const char* mock_audio_pattern_name(mock_audio_pattern_t pattern) {
    switch (pattern) {
        case PATTERN_MUSIC: return "Music";
        case PATTERN_SWEEP: return "Sweep";
        case PATTERN_NOISE: return "Noise";
        case PATTERN_BASS:  return "Bass";
        case PATTERN_TONE:  return "Tone";
        case PATTERN_VU:    return "VU Meter";
        case PATTERN_AUTO:  return "Auto";
        default:            return "Unknown";
    }
}

uint32_t mock_audio_frame_count(void) {
    return _frame_count;
}

