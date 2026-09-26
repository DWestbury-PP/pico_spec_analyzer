/**
 * @file renderer.h
 * @brief Core 1: theme management, overlays, input and frame streaming.
 */

#ifndef RENDERER_H
#define RENDERER_H

#include <stdint.h>

typedef struct {
    uint32_t sys_hz;
    uint32_t spi_hz;
    int      selftest_bad;   // -1: readback unavailable
    uint8_t  pixfmt;         // RDPIXFMT readback (0x05 expected)
} boot_info_t;

extern boot_info_t g_boot_info;

/** Core 1 entry point. Never returns. */
void renderer_run(void);

#endif // RENDERER_H
