/**
 * @file waterfall.c
 * @brief Scrolling spectrogram: one row per two analysis hops (93.75 rows/s,
 *        2.4 s of history), perceptual "inferno" colour map, newest row on top.
 */

#include "display/theme.h"
#include "display/gfx.h"
#include "platform.h"
#include <math.h>
#include <string.h>

#define ROWS          224
#define HOPS_PER_ROW  2
#define LABEL_Y       (FB_H - 9)

static uint8_t (*const _ring)[SPEC_COLS] = (uint8_t (*)[SPEC_COLS])g_theme_arena;
static int _head;
static int _acc_n;
static uint8_t _acc[SPEC_COLS];
static uint8_t _lut[256];

static void enter(void) {
    static const uint32_t inferno[] = { 0x000004, 0x1B0C41, 0x4A0C6B, 0x781C6D, 0xA52C60,
                                        0xCF4446, 0xED6925, 0xFB9B06, 0xF7D13D, 0xFCFFA4 };
    gfx_ramp(1, 119, inferno, 10);
    for (int v = 0; v < 256; v++) {
        _lut[v] = (uint8_t)(1 + lroundf(powf(v / 255.0f, 1.25f) * 118.0f));
    }
    memset(g_theme_arena, 1, ROWS * SPEC_COLS);
    memset(_acc, 0, sizeof(_acc));
    _head = 0;
    _acc_n = 0;
}

static void on_hop(const spectrum_hop_t *h) {
    for (int c = 0; c < SPEC_COLS; c++) if (h->level[c] > _acc[c]) _acc[c] = h->level[c];
    if (++_acc_n < HOPS_PER_ROW) return;
    uint8_t *row = _ring[_head];
    for (int c = 0; c < SPEC_COLS; c++) row[c] = _lut[_acc[c]];
    _head = (_head + 1) % ROWS;
    _acc_n = 0;
    memset(_acc, 0, sizeof(_acc));
}

static void RAMFUNC(draw)(const spectrum_frame_t *f) {
    (void)f;
    for (int r = 0; r < ROWS; r++) {
        int src = (_head - 1 - r + ROWS) % ROWS;
        memcpy(&g_fb[r * FB_W], _ring[src], SPEC_COLS);
    }
    memset(&g_fb[ROWS * FB_W], 0, (FB_H - ROWS) * FB_W);
    for (int i = 0; i < 8; i++) {
        static const float hz[8] = { 50, 100, 200, 500, 1000, 2000, 5000, 10000 };
        gfx_vline(theme_hz_x(hz[i]), ROWS, ROWS + 2, UI_GREY);
    }
    theme_freq_labels(LABEL_Y, UI_GREY);
}

const theme_t theme_waterfall = {
    .name = "SPECTROGRAM",
    .enter = enter,
    .on_hop = on_hop,
    .draw = draw,
};
