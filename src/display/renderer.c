/**
 * @file renderer.c
 * @brief Core 1 frame loop.
 *
 * Double-buffered and overlapped: frame N+1 is drawn into the back buffer while
 * frame N streams from the front buffer by DMA IRQ. The frame period is
 * max(draw, stream), and the SPI transfer is ~19.8 ms at 62.5 MHz.
 *
 *   stream N  |=====================================|
 *   draw N+1                         |-- draw --|  ^ swap, stream N+1
 *                                    ^ just-in-time start (latest mailbox)
 *
 * Starting the draw just in time keeps sample-to-photon latency at about
 * draw + one transfer, while the frame rate is set by the transfer alone.
 */

#include "display/renderer.h"
#include "display/theme.h"
#include "display/gfx.h"
#include "display/ili9341.h"
#include "core/crash.h"
#include "core/shared.h"
#include "touch/xpt2046.h"
#include "config.h"
#include "pico/stdlib.h"
#include <stdio.h>
#include <string.h>

boot_info_t g_boot_info;

static const theme_t *const _themes[] = {
    &theme_bars,
    &theme_rta,
    &theme_waterfall,
    &theme_terrain,
    &theme_radial,
    &theme_scope,
};
#define NTHEMES (int)(sizeof(_themes) / sizeof(_themes[0]))

static const char *const _test_names[TEST_COUNT] = { "MIC", "TEST 440 HZ", "TEST 1 KHZ", "TEST SWEEP", "TEST CHORD" };

static int _theme_idx;
static int _pending_theme = -1;
static bool _hud = true;
static bool _dump_pending;
static char _popup[32];
static uint64_t _popup_until;

static spectrum_frame_t _frame;

#define JIT_MARGIN_US 700   // slack between finishing a draw and the stream completing

// Stats (reset every report)
typedef struct {
    uint32_t frames;
    uint32_t draw_us_sum, draw_us_max;
    uint32_t stream_us_sum;
    uint32_t lat_us_sum, lat_us_max;
    uint32_t lat_n;
} stats_t;
static stats_t _st;
static float _fps, _draw_ms, _lat_ms, _stream_ms;

// ============================================================================
// Input
// ============================================================================

static void popup(const char *msg, uint32_t ms) {
    snprintf(_popup, sizeof(_popup), "%s", msg);
    _popup_until = time_us_64() + (uint64_t)ms * 1000;
}

static void select_theme(int idx) {
    _pending_theme = (idx % NTHEMES + NTHEMES) % NTHEMES;
}

static void cycle_test_signal(void) {
    uint8_t t = (uint8_t)((g_audio_ctl.test_signal + 1) % TEST_COUNT);
    g_audio_ctl.test_signal = t;
    popup(_test_names[t], 1500);
    printf("input: %s\n", _test_names[t]);
}

static void print_status(void);

static void handle_input(void) {
    switch (xpt2046_detect_gesture()) {
        case GESTURE_SWIPE_RIGHT: case GESTURE_SWIPE_DOWN: select_theme(_theme_idx + 1); break;
        case GESTURE_SWIPE_LEFT:  case GESTURE_SWIPE_UP:   select_theme(_theme_idx - 1); break;
        case GESTURE_TAP:         _hud = !_hud; printf("touch: tap\n"); break;
        case GESTURE_LONG_PRESS:  cycle_test_signal(); break;
        default: break;
    }

    int ch;
    while ((ch = getchar_timeout_us(0)) != PICO_ERROR_TIMEOUT) {
        switch (ch) {
            case 'n': select_theme(_theme_idx + 1); break;
            case 'p': select_theme(_theme_idx - 1); break;
            case 'h': _hud = !_hud; break;
            case 't': cycle_test_signal(); break;
            case 'a':
                g_audio_ctl.agc = !g_audio_ctl.agc;
                popup(g_audio_ctl.agc ? "AGC ON" : "AGC OFF", 1200);
                break;
            case 'f': _dump_pending = true; break;
            case 's': print_status(); break;
            default:
                if (ch >= '1' && ch < '1' + NTHEMES) select_theme(ch - '1');
                break;
        }
    }
}

// ============================================================================
// Overlays
// ============================================================================

static void draw_hud(void) {
    char l1[40], l2[40];
    snprintf(l1, sizeof(l1), "%4.1f FPS  LAT %2.0f MS", (double)_fps, (double)_lat_ms);
    snprintf(l2, sizeof(l2), "DSP %.2f  DRAW %.1f MS", _frame.dsp_us_avg / 1000.0, (double)_draw_ms);
    int w = gfx_text_width(l2, 1);
    int w1 = gfx_text_width(l1, 1);
    if (w1 > w) w = w1;
    gfx_overlay_rect(0, 0, w + 6, 21);
    gfx_dim_rect(0, 0, w + 6, 21);
    gfx_text(3, 2, l1, UI_GOOD, 1);
    gfx_text(3, 11, l2, UI_LIGHT, 1);
}

static void draw_popup(void) {
    if (time_us_64() >= _popup_until) return;
    int w = gfx_text_width(_popup, 2);
    int x = (FB_W - w) / 2, y = FB_H / 2 - 7;
    gfx_overlay_rect(x - 12, y - 7, w + 24, 28);
    gfx_dim_rect(x - 12, y - 7, w + 24, 28);
    gfx_hline(x - 12, x + w + 11, y - 7, UI_ACCENT);
    gfx_hline(x - 12, x + w + 11, y + 20, UI_ACCENT);
    gfx_text(x, y, _popup, UI_WHITE, 2);
}

// ============================================================================
// Framebuffer dump (serial): palette + indexed pixels, base64
// ============================================================================

static void dump_framebuffer(void) {
    static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    const uint32_t pal_bytes = 256 * sizeof(uint16_t), total = pal_bytes + FB_BYTES;
    char line[80];
    int n = 0;
    printf("\n#FB %d %d\n", FB_W, FB_H);
    for (uint32_t i = 0; i < total; i += 3) {
        uint8_t b[3];
        for (int k = 0; k < 3; k++) {
            uint32_t j = i + (uint32_t)k;
            b[k] = j >= total ? 0 : j < pal_bytes ? ((const uint8_t *)g_palette)[j] : g_fb[j - pal_bytes];
        }
        uint32_t v = ((uint32_t)b[0] << 16) | ((uint32_t)b[1] << 8) | b[2];
        line[n++] = b64[(v >> 18) & 63];
        line[n++] = b64[(v >> 12) & 63];
        line[n++] = i + 1 < total ? b64[(v >> 6) & 63] : '=';
        line[n++] = i + 2 < total ? b64[v & 63] : '=';
        if (n >= 76) { line[n] = 0; puts(line); n = 0; }
    }
    if (n) { line[n] = 0; puts(line); }
    printf("#END\n");
}

// ============================================================================
// Stats
// ============================================================================

static void print_status(void) {
    if (g_crash.fault)
        printf("LAST RESET: HardFault on core %lu at pc=0x%08lx lr=0x%08lx (streak %lu)\n",
               (unsigned long)g_crash.core, (unsigned long)g_crash.pc, (unsigned long)g_crash.lr,
               (unsigned long)g_crash.streak);
    else if (g_crash.hang)
        printf("LAST RESET: watchdog (hang), streak %lu\n", (unsigned long)g_crash.streak);
    printf("sys %lu MHz | spi %.1f MHz | selftest %s%d | pixfmt 0x%02X | theme %s | input %s | agc %s\n",
           (unsigned long)(g_boot_info.sys_hz / 1000000), g_boot_info.spi_hz / 1e6,
           g_boot_info.selftest_bad < 0 ? "n/a " : "bad=", g_boot_info.selftest_bad,
           g_boot_info.pixfmt, _themes[_theme_idx]->name, _test_names[g_audio_ctl.test_signal],
           g_audio_ctl.agc ? "on" : "off");
}

static void report(uint64_t now, uint64_t *last) {
    static bool first = true;
    uint64_t span = now - *last;
    if (span < 2000000) return;
    if (first) { print_status(); first = false; }
    _fps = _st.frames * 1e6f / (float)span;
    _draw_ms = _st.frames ? _st.draw_us_sum / 1000.0f / _st.frames : 0;
    _stream_ms = _st.frames ? _st.stream_us_sum / 1000.0f / _st.frames : 0;
    _lat_ms = _st.lat_n ? _st.lat_us_sum / 1000.0f / _st.lat_n : 0;
    printf("fps %.1f | draw %.2f/%.2f ms | stream %.2f ms | lat %.1f/%.1f ms | dsp %u/%u us | "
           "skip %lu | qdrop %lu | peak %.1f Hz %.1f dBFS | top %.1f\n",
           (double)_fps, (double)_draw_ms, _st.draw_us_max / 1000.0, (double)_stream_ms,
           (double)_lat_ms, _st.lat_us_max / 1000.0, _frame.dsp_us_avg, _frame.dsp_us_max,
           (unsigned long)_frame.hops_skipped, (unsigned long)_frame.queue_drops,
           (double)_frame.peak_hz, (double)_frame.peak_dbfs, (double)_frame.top_dbfs);
    memset(&_st, 0, sizeof(_st));
    *last = now;
}

// ============================================================================
// Main loop
// ============================================================================

void renderer_run(void) {
    ili9341_stream_init();
    crash_watchdog_start();
    const theme_t *theme = _themes[0];
    theme->enter();
    popup(theme->name, 1500);

    uint32_t seen = 0;
    bool streaming = false;
    uint64_t stream_start = 0, stream_t_newest = 0;
    uint32_t stream_us = 20000, draw_est_us = 4000;
    uint64_t last = time_us_64(), last_report = last;

    while (true) {
        // ---- frame N is streaming from the front buffer ----
        const spectrum_hop_t *h;
        while ((h = hopq_peek()) != NULL) {
            if (theme->on_hop) theme->on_hop(h);
            hopq_pop();
        }
        handle_input();

        // Just-in-time start: finish drawing N+1 about when N finishes streaming,
        // so the spectrum we draw is as fresh as possible.
        if (streaming) {
            uint64_t start_at = stream_start + stream_us - draw_est_us - JIT_MARGIN_US;
            while (ili9341_stream_busy() && time_us_64() < start_at) {
                while ((h = hopq_peek()) != NULL) {
                    if (theme->on_hop) theme->on_hop(h);
                    hopq_pop();
                }
            }
        }

        // ---- draw frame N+1 into the back buffer ----
        uint64_t now = time_us_64();
        float dt = (now - last) / 1e6f;
        last = now;
        mailbox_read(&_frame, &seen);
        crash_heartbeat(_frame.hop);

        if (_pending_theme >= 0) {
            _theme_idx = _pending_theme;
            _pending_theme = -1;
            theme = _themes[_theme_idx];
            theme->enter();
            gfx_invalidate_front();
            popup(theme->name, 1500);
            printf("theme: %s\n", theme->name);
        }

        uint64_t t0 = time_us_64();
        if (theme->update) theme->update(&_frame, dt);
        theme->draw(&_frame);
        if (_hud) draw_hud();
        draw_popup();
        uint32_t draw_us = (uint32_t)(time_us_64() - t0);
        _st.draw_us_sum += draw_us;
        if (draw_us > _st.draw_us_max) _st.draw_us_max = draw_us;
        draw_est_us = draw_us > draw_est_us ? draw_us : draw_est_us - (draw_est_us >> 6);

        if (_dump_pending) {
            dump_framebuffer();
            _dump_pending = false;
        }

        // ---- frame N done: account for it, then stream N+1 ----
        ili9341_stream_wait();
        if (streaming) {
            uint64_t glass = ili9341_stream_done_us();
            stream_us = (uint32_t)(glass - stream_start);
            _st.stream_us_sum += stream_us;
            uint32_t lat = (uint32_t)(glass - stream_t_newest);
            _st.lat_us_sum += lat;
            _st.lat_n++;
            if (lat > _st.lat_us_max) _st.lat_us_max = lat;
        }

        gfx_swap();
        stream_start = time_us_64();
        stream_t_newest = _frame.t_newest_us;
        ili9341_stream_begin(gfx_front_fb(), gfx_front_palette());
        streaming = true;
        _st.frames++;

        report(stream_start, &last_report);
    }
}
