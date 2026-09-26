/**
 * @file radial.c
 * @brief "Nova": 96 mirrored spokes on a rotating ring, hue by frequency,
 *        motion trails via palette-index fading, bass-pulsed core and
 *        beat-triggered shockwaves.
 *
 * Instead of clearing, each frame remaps every pixel one brightness step down
 * inside its hue, so spokes leave fading trails as the ring spins.
 */

#include "display/theme.h"
#include "display/gfx.h"
#include "platform.h"
#include <math.h>
#include <string.h>

#define HUES        11
#define BRIGHT      10                    // idx = 1 + hue*10 + b  (1..110)
#define CORE_FIRST  111
#define CORE_N      9
#define NSPOKE      48                    // per half
#define CX          (FB_W / 2)
#define CY          (FB_H / 2)
#define R_MAX       116

static uint8_t _fade[256];
static float _rot, _bass, _bass_avg, _cool;
static float _shock_r;
static bool _shock;

static uint32_t hsv(float h, float s, float v) {
    float r, g, b;
    int i = (int)(h * 6.0f);
    float f = h * 6.0f - i, p = v * (1 - s), q = v * (1 - f * s), t = v * (1 - (1 - f) * s);
    switch (i % 6) {
        case 0: r = v; g = t; b = p; break;
        case 1: r = q; g = v; b = p; break;
        case 2: r = p; g = v; b = t; break;
        case 3: r = p; g = q; b = v; break;
        case 4: r = t; g = p; b = v; break;
        default: r = v; g = p; b = q; break;
    }
    return ((uint32_t)(r * 255) << 16) | ((uint32_t)(g * 255) << 8) | (uint32_t)(b * 255);
}

static void enter(void) {
    for (int h = 0; h < HUES; h++) {
        uint32_t c = hsv(0.62f - 0.62f * h / (HUES - 1), 0.85f, 1.0f);   // bass blue .. treble red
        if (h == HUES - 1) c = 0xFFF0F6;      // top hue doubles as the shockwave white
        for (int b = 0; b < BRIGHT; b++) {
            float k = (b + 1) / (float)BRIGHT;
            gfx_set_color((uint8_t)(1 + h * BRIGHT + b), gfx_scale_rgb(c, k * k));
        }
    }
    static const uint32_t core[] = { 0x1A0630, 0x5B1A8A, 0xC23BD4, 0xFF9CF0, 0xFFFFFF };
    gfx_ramp(CORE_FIRST, CORE_N, core, 5);

    memset(_fade, 0, sizeof(_fade));
    for (int i = 1; i <= HUES * BRIGHT; i++) {
        int b = (i - 1) % BRIGHT;
        _fade[i] = b > 0 ? (uint8_t)(i - 1) : 0;
    }
    gfx_clear(0);
    _rot = 0.0f;
    _shock = false;
}

static void update(const spectrum_frame_t *f, float dt) {
    float loud = f->loud / (float)LEVEL_MAX;
    _rot += dt * (0.15f + 1.1f * loud);
    float b = f->bass / (float)LEVEL_MAX;
    _bass += (b - _bass) * (dt * 20.0f > 1.0f ? 1.0f : dt * 20.0f);
    _bass_avg += (b - _bass_avg) * dt * 1.5f;
    _cool -= dt;
    if (!_shock && _cool <= 0.0f && b > 0.35f && b > _bass_avg * 1.25f) {
        _shock = true;
        _shock_r = 30.0f;
        _cool = 0.3f;
    }
    if (_shock) {
        _shock_r += 330.0f * dt;
        if (_shock_r > 200.0f) _shock = false;
    }
}

static void spoke(float ang, float r0, float r1, uint8_t idx) {
    float c = cosf(ang), s = sinf(ang);
    int x0 = CX + (int)(r0 * c), y0 = CY + (int)(r0 * s);
    int x1 = CX + (int)(r1 * c), y1 = CY + (int)(r1 * s);
    gfx_line(x0, y0, x1, y1, idx);
    // Second line offset along the perpendicular for a 2 px stroke
    int ox = (int)lroundf(-s), oy = (int)lroundf(c);
    gfx_line(x0 + ox, y0 + oy, x1 + ox, y1 + oy, idx);
}

static void RAMFUNC(draw)(const spectrum_frame_t *f) {
    gfx_remap(_fade);

    const float r0 = 24.0f + _bass * 20.0f;
    for (int j = 0; j < NSPOKE; j++) {
        uint32_t lv = 0;
        for (int c = j * SPEC_COLS / NSPOKE; c < (j + 1) * SPEC_COLS / NSPOKE; c++)
            if (f->level[c] > lv) lv = f->level[c];
        float len = (float)lv / LEVEL_MAX * (R_MAX - r0);
        if (len < 1.0f) continue;
        uint8_t idx = (uint8_t)(1 + (j * (HUES - 1) / NSPOKE) * BRIGHT + BRIGHT - 1);
        float a = (j + 0.5f) * (float)M_PI / NSPOKE;
        spoke(-(float)M_PI / 2 + a + _rot, r0, r0 + len, idx);
        spoke(-(float)M_PI / 2 - a + _rot, r0, r0 + len, idx);
    }

    if (_shock) gfx_circle(CX, CY, (int)_shock_r, (uint8_t)(1 + (HUES - 1) * BRIGHT + BRIGHT - 1));

    // Core: concentric glow, brighter with bass
    int rc = (int)r0 - 4;
    for (int r = rc; r > 0; r -= 3) {
        int k = (int)((1.0f - (float)r / rc) * 4.0f + _bass * 4.0f);
        if (k > CORE_N - 1) k = CORE_N - 1;
        gfx_fill_circle(CX, CY, r, (uint8_t)(CORE_FIRST + k));
    }
    gfx_circle(CX, CY, (int)r0 - 2, CORE_FIRST + CORE_N - 1);
}

const theme_t theme_radial = {
    .name = "NOVA",
    .enter = enter,
    .update = update,
    .draw = draw,
};
