/**
 * @file fft_processor.h
 * @brief FFT processing and frequency band extraction
 * 
 * Performs FFT on audio samples and extracts frequency bands
 * for spectrum visualization.
 */

#ifndef FFT_PROCESSOR_H
#define FFT_PROCESSOR_H

#include <stdint.h>
#include <stdbool.h>

// FFT configuration
#define FFT_SIZE 64  // Must match config.h

/**
 * @brief Initialize FFT processor
 * @param sample_rate_hz Sample rate in Hz
 * @return true if successful
 */
bool fft_processor_init(uint32_t sample_rate_hz);

/**
 * @brief Process audio samples and extract frequency bands
 * @param samples Input audio samples (FFT_SIZE samples)
 * @param bands Output frequency band amplitudes (0.0 to 1.0)
 * @param num_bands Number of frequency bands to extract
 * @return true if successful
 */
bool fft_processor_compute(const uint16_t *samples, float *bands, uint8_t num_bands);

/**
 * @brief Get frequency range for a specific band
 * @param band_index Band index (0 to num_bands-1)
 * @param num_bands Total number of bands
 * @param freq_min Output: minimum frequency in Hz
 * @param freq_max Output: maximum frequency in Hz
 */
void fft_processor_get_band_range(uint8_t band_index, uint8_t num_bands, 
                                   float *freq_min, float *freq_max);

#endif // FFT_PROCESSOR_H

