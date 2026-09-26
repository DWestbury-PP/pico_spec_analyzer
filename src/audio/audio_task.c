/**
 * @file audio_task.c
 * @brief Core 0: free-running ADC -> DMA ring -> CIC + dual FFT -> mailbox.
 *
 * The ADC paces itself from its 48 MHz clock (divider 1000 = exactly 48 kHz),
 * and a data DMA channel writes into an aligned ring. When a pass completes,
 * a control channel rewrites the data channel's write address and retriggers it,
 * so capture never stops and costs no CPU per sample.
 */

#include "audio/audio_task.h"
#include "audio/dsp.h"
#include "core/shared.h"
#include "config.h"
#include "platform.h"
#include "hardware/adc.h"
#include "hardware/dma.h"
#include "pico/time.h"
#include <math.h>
#include <string.h>

#define RING_LOG2   12
#define RING_N      (1 << RING_LOG2)
#define RING_MASK   (RING_N - 1)
#define LO_RING_N   512                   // CIC output history (power of two >= LO_N)
#define STATS_HOPS  375                   // 2 s window for dsp_us_max

static uint16_t _ring[RING_N] __attribute__((aligned(RING_N * sizeof(uint16_t))));
static uint16_t *_ring_base = _ring;      // source word for the re-arm DMA
static int _dma_data = -1;
static int _dma_ctrl = -1;

static cic_t    _cic;
static int32_t  _lo_ring[LO_RING_N];
static uint32_t _lo_head;

static uint16_t _hi[HI_N];
static int32_t  _lo[LO_N];
static dsp_result_t _res;
static levels_t _lv;
static spectrum_frame_t _frame;
static int _bass_cols;

// ============================================================================
// Capture
// ============================================================================

void audio_task_init(void) {
    adc_init();
    adc_gpio_init(AUDIO_PIN_MIC);
    adc_select_input(AUDIO_ADC_MIC);
    adc_fifo_setup(true, true, 1, false, false);   // FIFO on, DREQ on, no error bit, 16-bit
    adc_set_clkdiv(48000000.0f / AUDIO_FS_HZ - 1.0f);

    _dma_data = dma_claim_unused_channel(true);
    _dma_ctrl = dma_claim_unused_channel(true);

    dma_channel_config c = dma_channel_get_default_config(_dma_data);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_16);
    channel_config_set_read_increment(&c, false);
    channel_config_set_write_increment(&c, true);
    channel_config_set_ring(&c, true, RING_LOG2 + 1);   // wrap writes on the byte size
    channel_config_set_dreq(&c, DREQ_ADC);
    channel_config_set_chain_to(&c, _dma_ctrl);
    dma_channel_configure(_dma_data, &c, _ring, &adc_hw->fifo, RING_N, false);

    dma_channel_config k = dma_channel_get_default_config(_dma_ctrl);
    channel_config_set_transfer_data_size(&k, DMA_SIZE_32);
    channel_config_set_read_increment(&k, false);
    channel_config_set_write_increment(&k, false);
    dma_channel_configure(_dma_ctrl, &k, &dma_hw->ch[_dma_data].al2_write_addr_trig,
                          &_ring_base, 1, false);

    dsp_init();
    levels_init(&_lv);
    cic_reset(&_cic);
    _bass_cols = (int)spectrum_hz_col(150.0f) + 1;

    dma_channel_start(_dma_data);
    adc_run(true);
}

static inline uint32_t ring_write_index(void) {
    return ((dma_hw->ch[_dma_data].write_addr - (uint32_t)_ring) >> 1) & RING_MASK;
}

// ============================================================================
// Test signals (written into the ring in place of mic samples)
// ============================================================================

static int16_t  _sine[1024];
static uint32_t _phase[5];
static uint32_t _test_samples;

static void test_init(void) {
    for (int i = 0; i < 1024; i++) _sine[i] = (int16_t)lroundf(32767.0f * sinf(2.0f * (float)M_PI * i / 1024));
}

static inline uint32_t phase_inc(float hz) {
    return (uint32_t)(hz * (4294967296.0f / AUDIO_FS_HZ));
}

static void RAMFUNC(test_fill)(uint32_t pos, uint32_t n, uint8_t sig) {
    static const float chord[5] = { 55.0f, 220.0f, 880.0f, 3520.0f, 10000.0f };
    uint32_t inc[5] = {0};
    int voices = 1;
    int32_t amp = 514;                       // -12 dBFS
    switch (sig) {
        case TEST_440:   inc[0] = phase_inc(440.0f); break;
        case TEST_1K:    inc[0] = phase_inc(1000.0f); break;
        case TEST_SWEEP: {
            float t = (_test_samples % (8 * AUDIO_FS_HZ)) / (float)AUDIO_FS_HZ;
            inc[0] = phase_inc(SPEC_FMIN_HZ * powf(SPEC_FMAX_HZ / SPEC_FMIN_HZ, t / 8.0f));
            break;
        }
        case TEST_CHORD:
            voices = 5;
            amp = 205;                       // -20 dBFS each
            for (int v = 0; v < 5; v++) inc[v] = phase_inc(chord[v]);
            break;
        default: return;
    }
    for (uint32_t i = 0; i < n; i++) {
        int32_t s = 0;
        for (int v = 0; v < voices; v++) {
            s += _sine[_phase[v] >> 22];
            _phase[v] += inc[v];
        }
        _ring[(pos + i) & RING_MASK] = (uint16_t)(2048 + ((s * amp) >> 15));
    }
    _test_samples += n;
}

// ============================================================================
// Analysis loop
// ============================================================================

/** Copy the newest waveform, aligned to a rising zero crossing so the scope is stable. */
static void RAMFUNC(capture_wave)(void) {
    int32_t sum = 0, pk = 0;
    for (int i = 0; i < HI_N; i++) sum += _hi[i];
    const int32_t mean = sum >> HI_LOG2;
    for (int i = HI_N - 2 * SCOPE_N; i < HI_N; i++) {
        int32_t a = _hi[i] - mean;
        if (a < 0) a = -a;
        if (a > pk) pk = a;
    }
    const int32_t hyst = pk > 64 ? pk / 8 : 8;
    int trig = HI_N - SCOPE_N;
    for (int t = HI_N - SCOPE_N; t > 8; t--) {
        if ((int32_t)_hi[t - 1] - mean < 0 && (int32_t)_hi[t] - mean >= 0 &&
            (int32_t)_hi[t - 8] - mean < -hyst) {
            trig = t;
            break;
        }
    }
    for (int i = 0; i < SCOPE_N; i++) _frame.wave[i] = (int16_t)(_hi[trig + i] - mean);
}

void RAMFUNC(audio_task_run)(void) {
    test_init();

    uint32_t rd = ring_write_index();
    uint32_t hop = 0;
    uint32_t skipped = 0;
    uint32_t dsp_avg_x16 = 0, dsp_max = 0, dsp_max_win = 0, win_hops = 0;

    while (true) {
        uint32_t avail = (ring_write_index() - rd) & RING_MASK;
        if (avail < HOP_SAMPLES) continue;

        // Hopelessly behind (would read samples DMA is overwriting): resync.
        if (avail > RING_N - HI_N - HOP_SAMPLES) {
            rd = (ring_write_index() - HOP_SAMPLES) & RING_MASK;
            avail = HOP_SAMPLES;
            skipped++;
        }

        // Consume every available hop through the CIC; analyse only the newest.
        uint32_t hops_now = 0;
        const uint8_t sig = g_audio_ctl.test_signal;
        while (avail >= HOP_SAMPLES) {
            if (sig != TEST_OFF) test_fill(rd, HOP_SAMPLES, sig);
            for (uint32_t i = 0; i < HOP_SAMPLES; i++) {
                int32_t o;
                if (cic_push(&_cic, (int32_t)_ring[(rd + i) & RING_MASK] - 2048, &o)) {
                    _lo_ring[_lo_head++ & (LO_RING_N - 1)] = o;
                }
            }
            rd = (rd + HOP_SAMPLES) & RING_MASK;
            avail -= HOP_SAMPLES;
            hops_now++;
        }
        skipped += hops_now - 1;
        const uint64_t t_newest = time_us_64();
        const uint64_t t0 = t_newest;

        for (int i = 0; i < HI_N; i++) _hi[i] = _ring[(rd - HI_N + i) & RING_MASK];
        for (int i = 0; i < LO_N; i++) _lo[i] = _lo_ring[(_lo_head - LO_N + i) & (LO_RING_N - 1)];

        dsp_analyze(_hi, _lo, &_res);

        _lv.agc = g_audio_ctl.agc;
        spectrum_hop_t *slot = hopq_write_slot();
        float top, range;
        levels_update(&_lv, _res.col_db, slot ? slot->level : NULL, &top, &range);
        hop++;
        if (slot) {
            slot->hop = hop;
            slot->t_newest_us = t_newest;
            hopq_commit();
        }

        _frame.hop = hop;
        _frame.t_newest_us = t_newest;
        memcpy(_frame.level, _lv.bal, sizeof(_frame.level));
        capture_wave();
        _frame.top_dbfs = top;
        _frame.range_db = range;
        _frame.peak_hz = _res.peak_hz;
        _frame.peak_dbfs = _res.peak_dbfs;
        uint32_t bass = 0, loud = 0;
        for (int c = 0; c < SPEC_COLS; c++) {
            loud += _lv.bal[c];
            if (c < _bass_cols) bass += _lv.bal[c];
        }
        _frame.bass = (uint16_t)(bass / _bass_cols);
        _frame.loud = (uint16_t)(loud / SPEC_COLS);

        uint32_t us = (uint32_t)(time_us_64() - t0);
        dsp_avg_x16 = dsp_avg_x16 ? dsp_avg_x16 + us - (dsp_avg_x16 >> 4) : us << 4;
        if (us > dsp_max_win) dsp_max_win = us;
        if (++win_hops >= STATS_HOPS) {
            dsp_max = dsp_max_win;
            dsp_max_win = 0;
            win_hops = 0;
        }
        _frame.dsp_us_avg = (uint16_t)(dsp_avg_x16 >> 4);
        _frame.dsp_us_max = (uint16_t)(dsp_max > dsp_max_win ? dsp_max : dsp_max_win);
        _frame.hops_skipped = skipped;
        _frame.queue_drops = hopq_drops();

        mailbox_publish(&_frame);
    }
}
