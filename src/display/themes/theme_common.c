/**
 * @file theme_common.c
 * @brief Axis helpers shared by the spectrum themes.
 */

#include "display/theme.h"
#include "display/gfx.h"
#include <math.h>
#include <stdio.h>

uint8_t g_theme_arena[THEME_ARENA_BYTES] __attribute__((aligned(4)));

static const struct { float hz; const char *label; bool major; } _ticks[] = {
    {    50.0f, "50",  false },
    {   100.0f, "100", true  },
    {   200.0f, "200", false },
    {   500.0f, "500", false },
    {  1000.0f, "1k",  true  },
    {  2000.0f, "2k",  false },
    {  5000.0f, "5k",  false },
    { 10000.0f, "10k", true  },
};
#define NTICKS (int)(sizeof(_ticks) / sizeof(_ticks[0]))

int theme_hz_x(float hz) {
    return (int)lroundf(spectrum_hz_col(hz));
}

void theme_freq_labels(int y, uint8_t idx) {
    for (int i = 0; i < NTICKS; i++) {
        int x = theme_hz_x(_ticks[i].hz) - gfx_text_width(_ticks[i].label, 1) / 2;
        gfx_text(x, y, _ticks[i].label, idx, 1);
    }
}

void theme_freq_grid(int y0, int y1, uint8_t major, uint8_t minor) {
    for (int i = 0; i < NTICKS; i++) {
        gfx_vline(theme_hz_x(_ticks[i].hz), y0, y1, _ticks[i].major ? major : minor);
    }
}

void theme_fmt_hz(char *buf, int len, float hz) {
    if (hz >= 1000.0f) snprintf(buf, (size_t)len, "%.2f kHz", hz / 1000.0f);
    else snprintf(buf, (size_t)len, "%.0f Hz", hz);
}
