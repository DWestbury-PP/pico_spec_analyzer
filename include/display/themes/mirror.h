/**
 * @file mirror.h
 * @brief Mirror mode visualization
 * 
 * Displays frequency bands as vertically mirrored bars.
 * Creates a symmetric stereo-like effect.
 */

#ifndef MIRROR_H
#define MIRROR_H

#include <stdint.h>

/**
 * @brief Initialize mirror visualization
 */
void mirror_init(void);

/**
 * @brief Render mirror visualization
 * @param bands Array of frequency band amplitudes (0.0 to 1.0)
 * @param num_bands Number of frequency bands
 */
void mirror_render(const float *bands, uint8_t num_bands);

/**
 * @brief Clear the display and reset state
 */
void mirror_clear(void);

#endif // MIRROR_H

