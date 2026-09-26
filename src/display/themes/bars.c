/**
 * @file bars.c
 * @brief 64-band bar analyser: height gradient, gravity peak caps, dithered floor reflection.
 */

#include "display/theme.h"
#include "display/gfx.h"
#include "platform.h"
#include <string.h>
#include <stdio.h>

#define NBARS      64
#define BAR_PITCH  (SPEC_COLS / NBARS)   // 5 columns: 4 lit + 1 gap
#define Y_TOP      14
#define Y_FLOOR    186
#define BAR_H      (Y_FLOOR - Y_TOP)
#define REFL_H     34
#define LABEL_Y    (FB_H - 9)

#define RAMP_FIRST 1
#define RAMP_N     100
#define CAP_IDX    110

#define PEAK_HOLD_S   0.45f
#define PEAK_GRAVITY  1400.0f            // px/s^2

static float _peak[NBARS];
static float _vel[NBARS];
static float _hold[NBARS];
static float _h[NBARS];

static void enter(void) {
    static const uint32_t sunset[] = { 0x2A0E6E, 0x6A1B9A, 0xD6249F, 0xFF4E50, 0xFF9A1F, 0xFFE259 };
    gfx_ramp(RAMP_FIRST, RAMP_N, sunset, 6);
    gfx_set_color(CAP_IDX, 0xFFF4D6);
    memset(_peak, 0, sizeof(_peak));
    memset(_vel, 0, sizeof(_vel));
    memset(_hold, 0, sizeof(_hold));
}

static void update(const spectrum_frame_t *f, float dt) {
    for (int b = 0; b < NBARS; b++) {
        uint32_t lv = 0;
        for (int c = b * BAR_PITCH; c < (b + 1) * BAR_PITCH; c++) if (f->level[c] > lv) lv = f->level[c];
        float h = (float)lv * BAR_H / LEVEL_MAX;
        _h[b] = h;
        if (h >= _peak[b]) {
            _peak[b] = h;
            _vel[b] = 0.0f;
            _hold[b] = PEAK_HOLD_S;
        } else if (_hold[b] > 0.0f) {
            _hold[b] -= dt;
        } else {
            _vel[b] += PEAK_GRAVITY * dt;
            _peak[b] -= _vel[b] * dt;
            if (_peak[b] < h) { _peak[b] = h; _vel[b] = 0.0f; }
        }
    }
}

static void RAMFUNC(draw)(const spectrum_frame_t *f) {
    gfx_clear(0);

    // dB grid every 6 dB
    for (float db = 6.0f; db < f->range_db; db += 6.0f) {
        int y = Y_FLOOR - (int)(db / f->range_db * BAR_H);
        for (int x = 0; x < FB_W; x += 2) g_fb[y * FB_W + x] = UI_GRID;
    }
    gfx_hline(0, FB_W - 1, Y_FLOOR + 1, UI_GRID_HI);

    for (int b = 0; b < NBARS; b++) {
        const int x0 = b * BAR_PITCH;
        int h = (int)(_h[b] + 0.5f);
        if (h > BAR_H) h = BAR_H;

        // Bar: 4 px per row, colour by height
        uint8_t *p = &g_fb[Y_FLOOR * FB_W + x0];
        for (int r = 0; r < h; r++, p -= FB_W) {
            uint8_t c = (uint8_t)(RAMP_FIRST + r * (RAMP_N - 1) / BAR_H);
            p[0] = c; p[1] = c; p[2] = c; p[3] = c;
        }

        // Reflection: mirrored, dimmed, dithered out with depth
        if (h > 0) {
            int rh = h * REFL_H / BAR_H + 2;
            if (rh > REFL_H) rh = REFL_H;
            uint8_t *q = &g_fb[(Y_FLOOR + 2) * FB_W + x0];
            for (int r = 0; r < rh; r++, q += FB_W) {
                uint8_t c = (uint8_t)(RAMP_FIRST + r * (RAMP_N - 1) / BAR_H) | GFX_DIM;
                int fade = r * 16 / REFL_H, y = Y_FLOOR + 2 + r;
                for (int i = 0; i < 4; i++) if (gfx_bayer4(x0 + i, y) >= fade) q[i] = c;
            }
        }

        if (_peak[b] > 1.0f) {
            int py = Y_FLOOR - (int)(_peak[b] + 0.5f);
            if (py < 1) py = 1;
            uint8_t *c = &g_fb[(py - 1) * FB_W + x0];
            c[0] = c[1] = c[2] = c[3] = CAP_IDX;
            c += FB_W;
            c[0] = c[1] = c[2] = c[3] = CAP_IDX;
        }
    }

    theme_freq_labels(LABEL_Y, UI_GREY);

    char buf[24];
    snprintf(buf, sizeof(buf), "%+.0f dBFS", (double)f->top_dbfs);
    gfx_text(FB_W - gfx_text_width(buf, 1) - 2, 3, buf, UI_GREY, 1);
}

const theme_t theme_bars = {
    .name = "SPECTRUM",
    .enter = enter,
    .update = update,
    .draw = draw,
};
