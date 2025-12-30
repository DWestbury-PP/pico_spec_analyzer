/**
 * @file adc_sampler.h
 * @brief ADC audio sampling for spectrum analyzer
 * 
 * Captures audio samples from ADC at specified sample rate.
 * Uses free-running ADC with timer-based sampling for consistent timing.
 */

#ifndef ADC_SAMPLER_H
#define ADC_SAMPLER_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Initialize ADC sampler
 * @param adc_channel ADC channel to sample (0 = GP26, 1 = GP27)
 * @param sample_rate_hz Desired sample rate in Hz
 * @return true if successful
 */
bool adc_sampler_init(uint8_t adc_channel, uint32_t sample_rate_hz);

/**
 * @brief Start sampling
 */
void adc_sampler_start(void);

/**
 * @brief Stop sampling
 */
void adc_sampler_stop(void);

/**
 * @brief Check if samples are available
 * @return Number of samples available
 */
uint32_t adc_sampler_available(void);

/**
 * @brief Read samples from buffer
 * @param buffer Output buffer for samples
 * @param count Number of samples to read
 * @return Number of samples actually read
 */
uint32_t adc_sampler_read(uint16_t *buffer, uint32_t count);

/**
 * @brief Get current sample rate
 * @return Sample rate in Hz
 */
uint32_t adc_sampler_get_rate(void);

#endif // ADC_SAMPLER_H

