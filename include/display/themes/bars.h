/**
 * @file bars.h
 * @brief Classic bar graph spectrum visualization
 */

#ifndef BARS_H
#define BARS_H

#include <stdint.h>

/**
 * @brief Initialize bars visualization
 */
void bars_init(void);

/**
 * @brief Render bars visualization
 * @param bands Array of frequency band amplitudes (0.0 to 1.0)
 * @param num_bands Number of frequency bands
 */
void bars_render(const float *bands, uint8_t num_bands);

/**
 * @brief Clear the display and reset state
 */
void bars_clear(void);

#endif // BARS_H

