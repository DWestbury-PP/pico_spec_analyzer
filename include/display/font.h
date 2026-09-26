/**
 * @file font.h
 * @brief 5x7 bitmap font (printable ASCII).
 */

#ifndef FONT_H
#define FONT_H

#include <stdint.h>

#define FONT_FIRST 0x20
#define FONT_LAST  0x7E

extern const uint8_t font5x7[95][5];

#endif // FONT_H
