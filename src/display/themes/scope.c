/**
 * @file scope.c
 * @brief Phosphor oscilloscope: rising-edge triggered waveform (6.7 ms window)
 *        with persistence, auto-ranging and a frequency readout.
 */

#include "display/theme.h"
#include "display/gfx.h"
#include "platform.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define PH_N        32                    // phosphor ramp 1..32 (brightness ordered)
#define TRACE_IDX   PH_N
#define GLOW_IDX    (PH_N - 10)
#define FADE_STEP   3
#define CY          (FB_H / 2)
#define HALF_H      100

static uint8_t _fade[256];
static float _env = 64.0f;

static void enter(void) {
    static const uint32_t ph[] = { 0x000000, 0x00210D, 0x006B2A, 0x19C95A, 0x9DFFC4, 0xF2FFF6 };
    gfx_ramp(1, PH_N, ph, 6);
    memset(_fade, 0, sizeof(_fade));
    for (int i = 1; i <= PH_N; i++) _fade[i] = i > FADE_STEP ? (uint8_t)(i - FADE_STEP) : 0;
    gfx_clear(0);
}

static void update(const spectrum_frame_t *f, float dt) {
    float pk = 0.0f;
    for (int i = 0; i < SCOPE_N; i++) {
        float a = fabsf((float)f->wave[i]);
        if (a > pk) pk = a;
    }
    float decay = expf(-dt * 1.5f);
    _env = pk > _env * decay ? pk : _env * decay;
    if (_env < 24.0f) _env = 24.0f;
}

static void RAMFUNC(draw)(const spectrum_frame_t *f) {
    gfx_copy_front();
    gfx_remap(_fade);

    // Graticule on empty pixels only, so trails stay on top
    for (int gx = 0; gx <= 10; gx++) {
        int x = gx * (FB_W - 1) / 10;
        for (int y = CY - HALF_H; y <= CY + HALF_H; y += (gx == 5 ? 2 : 4))
            if (g_fb[y * FB_W + x] == 0) g_fb[y * FB_W + x] = UI_GRID_HI;
    }
    for (int gy = -4; gy <= 4; gy++) {
        int y = CY + gy * HALF_H / 4;
        for (int x = 0; x < FB_W; x += (gy == 0 ? 2 : 4))
            if (g_fb[y * FB_W + x] == 0) g_fb[y * FB_W + x] = UI_GRID_HI;
    }

    const float gain = HALF_H * 0.9f / _env;
    int prev = CY - (int)(f->wave[0] * gain);
    for (int x = 0; x < SCOPE_N; x++) {
        int y = CY - (int)(f->wave[x] * gain);
        int lo = y < prev ? y : prev, hi = y < prev ? prev : y;
        gfx_px_max(x, lo - 1, GLOW_IDX);
        gfx_px_max(x, hi + 1, GLOW_IDX);
        for (int yy = lo; yy <= hi; yy++) gfx_px(x, yy, TRACE_IDX);
        prev = y;
    }

    char buf[40], hz[16];
    gfx_fill_rect(0, FB_H - 11, FB_W, 11, 0);
    theme_fmt_hz(hz, sizeof(hz), f->peak_hz);
    snprintf(buf, sizeof(buf), "%s  %.1f dBFS", hz, (double)f->peak_dbfs);
    gfx_text(3, FB_H - 9, buf, UI_GOOD, 1);
    snprintf(buf, sizeof(buf), "0.67 MS/DIV  %.0f CNT/DIV", (double)(_env / 0.9f / 4.0f));
    gfx_text(FB_W - gfx_text_width(buf, 1) - 3, FB_H - 9, buf, UI_GREY, 1);
}

const theme_t theme_scope = {
    .name = "SCOPE",
    .enter = enter,
    .update = update,
    .draw = draw,
};
