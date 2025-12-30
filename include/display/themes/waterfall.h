/**
 * @file waterfall.h
 * @brief Waterfall spectrogram visualization
 * 
 * Scrolling time-frequency display showing spectrum history.
 * Color intensity represents amplitude.
 */

#ifndef WATERFALL_H
#define WATERFALL_H

#include <stdint.h>

/**
 * @brief Initialize waterfall visualization
 */
void waterfall_init(void);

/**
 * @brief Render waterfall visualization
 * @param bands Array of frequency band amplitudes (0.0 to 1.0)
 * @param num_bands Number of frequency bands
 */
void waterfall_render(const float *bands, uint8_t num_bands);

/**
 * @brief Clear the display and reset state
 */
void waterfall_clear(void);

#endif // WATERFALL_H

