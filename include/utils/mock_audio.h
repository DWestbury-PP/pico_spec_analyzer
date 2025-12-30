/**
 * @file mock_audio.h
 * @brief Mock audio/FFT data generator for testing visualizations
 */

#ifndef MOCK_AUDIO_H
#define MOCK_AUDIO_H

#include <stdint.h>

// ============================================================================
// Pattern Types
// ============================================================================

typedef enum {
    PATTERN_MUSIC = 0,   // Music-like spectrum with bass emphasis
    PATTERN_SWEEP,       // Frequency sweep across bands
    PATTERN_NOISE,       // Random noise
    PATTERN_BASS,        // Bass-heavy pattern (kick drum)
    PATTERN_TONE,        // Single tone moving across bands
    PATTERN_VU,          // VU meter style (all bands same level)
    PATTERN_AUTO         // Automatically cycle through patterns
} mock_audio_pattern_t;

// ============================================================================
// Public API
// ============================================================================

/**
 * @brief Initialize mock audio generator
 */
void mock_audio_init(void);

/**
 * @brief Generate mock spectrum data
 * @param bands Output array for band amplitudes (0.0 to 1.0)
 * @param num_bands Number of frequency bands to generate
 * @param pattern Pattern to generate (or PATTERN_AUTO to cycle)
 */
void mock_audio_generate(float *bands, uint8_t num_bands, mock_audio_pattern_t pattern);

/**
 * @brief Get human-readable name for pattern
 * @param pattern Pattern type
 * @return String name of pattern
 */
const char* mock_audio_pattern_name(mock_audio_pattern_t pattern);

/**
 * @brief Get current frame count
 * @return Number of frames generated since init
 */
uint32_t mock_audio_frame_count(void);

#endif // MOCK_AUDIO_H

