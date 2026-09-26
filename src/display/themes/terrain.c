/**
 * @file terrain.c
 * @brief "Unknown Pleasures" meets synthwave: the last 40 spectra as ridge lines
 *        receding to a horizon, with a bass-pulsed striped sun behind them.
 *
 * Lines are drawn front to back against a per-column horizon buffer, so each
 * ridge is only drawn where nothing nearer already covers it. The area below a
 * visible ridge is cleared to black, so the mountains are opaque. Depth advances
 * by fractions of a line between hops, so the scroll is continuous.
 */

#include "display/theme.h"
#include "display/gfx.h"
#include "platform.h"
#include <math.h>
#include <string.h>

#define NLINES        40
#define NPTS          160
#define HOPS_PER_LINE 5
#define HORIZON_Y     92
#define FRONT_Y       234
#define AMP_FRONT     150.0f
#define PERSP_D       9.0f

#define DEPTHS        7
#define HEATS         16                  // idx = 1 + depth*16 + heat  (1..112)
#define SUN_FIRST     113
#define SUN_N         7

static uint8_t (*const _hist)[NPTS] = (uint8_t (*)[NPTS])g_theme_arena;
static int _head, _count, _acc_n;
static uint8_t _acc[NPTS];
static int16_t _horizon[FB_W];
static float _bass;

static void enter(void) {
    static const uint32_t heat[] = { 0x2B1055, 0x7B2FF7, 0xF72585, 0xFFB347, 0xFFF5B8 };
    for (int d = 0; d < DEPTHS; d++) {
        uint32_t tmp[5];
        float k = 1.0f - d * 0.12f;
        for (int i = 0; i < 5; i++) tmp[i] = gfx_scale_rgb(heat[i], k);
        gfx_ramp((uint8_t)(1 + d * HEATS), HEATS, tmp, 5);
    }
    static const uint32_t sun[] = { 0xFFE66D, 0xFF9F43, 0xFF4E88, 0xB0206E };
    gfx_ramp(SUN_FIRST, SUN_N, sun, 4);
    memset(g_theme_arena, 0, NLINES * NPTS);
    memset(_acc, 0, sizeof(_acc));
    _head = _count = _acc_n = 0;
}

static void on_hop(const spectrum_hop_t *h) {
    for (int i = 0; i < NPTS; i++) {
        uint8_t v = h->level[2 * i] > h->level[2 * i + 1] ? h->level[2 * i] : h->level[2 * i + 1];
        if (v > _acc[i]) _acc[i] = v;
    }
    if (++_acc_n < HOPS_PER_LINE) return;
    _head = (_head + 1) % NLINES;
    uint8_t *dst = _hist[_head];
    for (int i = 0; i < NPTS; i++) {          // 1-2-1 smoothing softens the ridges
        int a = _acc[i > 0 ? i - 1 : i], b = _acc[i], c = _acc[i < NPTS - 1 ? i + 1 : i];
        dst[i] = (uint8_t)((a + 2 * b + c) >> 2);
    }
    if (_count < NLINES) _count++;
    _acc_n = 0;
    memset(_acc, 0, sizeof(_acc));
}

static void update(const spectrum_frame_t *f, float dt) {
    float target = f->bass / (float)LEVEL_MAX;
    _bass += (target - _bass) * (dt * 12.0f > 1.0f ? 1.0f : dt * 12.0f);
}

static void draw_sun(void) {
    const int cx = FB_W / 2, cy = HORIZON_Y - 6;
    const int r = 34 + (int)(_bass * 14.0f);
    for (int dy = -r; dy <= 0; dy++) {
        int y = cy + dy;
        int from_bottom = -dy;
        // Stripes widen toward the horizon
        if (from_bottom < r / 2 && (from_bottom % 7) < 3 - from_bottom * 6 / r) continue;
        int hw = (int)sqrtf((float)(r * r - dy * dy));
        uint8_t idx = (uint8_t)(SUN_FIRST + (r + dy) * (SUN_N - 1) / r);
        gfx_hline(cx - hw, cx + hw, y, idx);
    }
    // Faint haze band at the horizon
    for (int y = HORIZON_Y - 10; y < HORIZON_Y; y++) {
        for (int x = 0; x < FB_W; x++) {
            if (g_fb[y * FB_W + x] == 0 && gfx_bayer4(x, y) < (y - (HORIZON_Y - 10)) + 2)
                g_fb[y * FB_W + x] = (SUN_FIRST + SUN_N - 1) | GFX_DIM;
        }
    }
}

static void RAMFUNC(draw)(const spectrum_frame_t *f) {
    (void)f;
    gfx_clear(0);
    draw_sun();

    for (int x = 0; x < FB_W; x++) _horizon[x] = FB_H;
    const float progress = (float)_acc_n / HOPS_PER_LINE;
    const float s_min = PERSP_D / (PERSP_D + NLINES);

    for (int k = 0; k < _count; k++) {
        const uint8_t *line = _hist[(_head - k + NLINES) % NLINES];
        const float z = k + progress;
        const float s = PERSP_D / (PERSP_D + z);
        const float sn = (s - s_min) / (1.0f - s_min);            // 1 front .. 0 horizon
        const int base = HORIZON_Y + (int)((FRONT_Y - HORIZON_Y) * sn);
        const float hw = 176.0f * (0.35f + 0.65f * sn);
        const int32_t amp_q8 = (int32_t)(AMP_FRONT * s * 256.0f / 255.0f);
        int depth = (int)(z * DEPTHS / NLINES);
        if (depth >= DEPTHS) depth = DEPTHS - 1;
        const uint8_t base_idx = (uint8_t)(1 + depth * HEATS);

        int xa = (int)ceilf(FB_W / 2 - hw), xb = (int)floorf(FB_W / 2 + hw);
        const int32_t step = (int32_t)((NPTS - 1) * 65536.0f / (2.0f * hw));
        int32_t u = (int32_t)((xa - (FB_W / 2 - hw)) * step);
        if (xa < 0) { u += -xa * step; xa = 0; }
        if (xb > FB_W - 1) xb = FB_W - 1;

        int prev_y = -1;
        for (int x = xa; x <= xb; x++, u += step) {
            int i = u >> 16;
            if (i >= NPTS - 1) i = NPTS - 2;
            int fr = (u >> 8) & 0xFF;
            int v = (line[i] * (256 - fr) + line[i + 1] * fr) >> 8;
            int y = base - ((v * amp_q8) >> 8);
            int hz = _horizon[x];
            if (y < hz) {
                int top = y, bot = y;
                if (prev_y >= 0) { if (prev_y < top) top = prev_y; if (prev_y > bot) bot = prev_y; }
                if (bot >= hz) bot = hz - 1;
                uint8_t idx = (uint8_t)(base_idx + (v * HEATS >> 8));
                if (top < 0) top = 0;
                uint8_t *p = &g_fb[top * FB_W + x];
                for (int yy = top; yy <= bot; yy++, p += FB_W) *p = idx;
                // Opaque body down to the nearer terrain (or this line's base)
                int fill_to = (base < hz ? base : hz - 1);
                for (int yy = bot + 1; yy <= fill_to; yy++, p += FB_W) *p = 0;
                _horizon[x] = (int16_t)y;
            }
            prev_y = y;
        }
    }
}

const theme_t theme_terrain = {
    .name = "TERRAIN",
    .enter = enter,
    .on_hop = on_hop,
    .update = update,
    .draw = draw,
};
