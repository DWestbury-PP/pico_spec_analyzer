/**
 * @file radial.h
 * @brief Radial/circular spectrum visualization
 * 
 * Displays frequency bands as bars radiating from center in a circle.
 * Creates a "blooming flower" or radar-like effect.
 */

#ifndef RADIAL_H
#define RADIAL_H

#include <stdint.h>

/**
 * @brief Initialize radial visualization
 */
void radial_init(void);

/**
 * @brief Render radial visualization
 * @param bands Array of frequency band amplitudes (0.0 to 1.0)
 * @param num_bands Number of frequency bands
 */
void radial_render(const float *bands, uint8_t num_bands);

/**
 * @brief Clear the display and reset state
 */
void radial_clear(void);

#endif // RADIAL_H

