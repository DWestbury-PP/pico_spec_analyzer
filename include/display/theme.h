/**
 * @file theme.h
 * @brief Visualisation theme interface.
 *
 * Frame lifecycle on core 1:
 *   on_hop()  for every queued hop    } run while the previous frame is still
 *   update()  once per frame (dt)     } streaming: must NOT touch g_fb/g_palette
 *   draw()    once per frame          - owns g_fb and g_palette
 */

#ifndef THEME_H
#define THEME_H

#include <stdint.h>
#include "audio/spectrum.h"

typedef struct {
    const char *name;
    void (*enter)(void);                                  // set palette, reset state
    void (*on_hop)(const spectrum_hop_t *hop);            // optional
    void (*update)(const spectrum_frame_t *f, float dt);  // optional
    void (*draw)(const spectrum_frame_t *f);
} theme_t;

extern const theme_t theme_bars;
extern const theme_t theme_rta;
extern const theme_t theme_waterfall;
extern const theme_t theme_terrain;
extern const theme_t theme_radial;
extern const theme_t theme_scope;

// Scratch memory shared by themes (only the active theme uses it).
#define THEME_ARENA_BYTES (320 * 224)
extern uint8_t g_theme_arena[THEME_ARENA_BYTES];

// --- Shared drawing helpers (theme_common.c) ---------------------------------

/** Frequency labels (50 .. 10k) centred on their log column, text top at y. */
void theme_freq_labels(int y, uint8_t idx);

/** Vertical gridlines at the labelled frequencies (major at 100/1k/10k). */
void theme_freq_grid(int y0, int y1, uint8_t major, uint8_t minor);

/** Pixel column for a frequency. */
int theme_hz_x(float hz);

/** Format a frequency compactly ("440 Hz", "1.02 kHz"). */
void theme_fmt_hz(char *buf, int len, float hz);

#endif // THEME_H
