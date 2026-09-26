/**
 * @file dsp.c
 * @brief Dual-resolution spectrum analysis in fixed point.
 *
 * Mids and highs come from a 1024-point FFT at 48 kHz (46.9 Hz bins, 21 ms
 * window). Bass comes from a 256-point FFT of a CIC-decimated 3 kHz stream
 * (11.7 Hz bins). Both are calibrated to dBFS for a sinusoid, then resampled
 * onto SPEC_COLS log-spaced columns: interpolated where bins are sparser than
 * columns, peak-picked where they are denser.
 */

#include "audio/dsp.h"
#include "audio/fft_q.h"
#include "platform.h"
#include <math.h>
#include <string.h>

#define HI_BINS      (HI_N / 2)
#define LO_BINS      (LO_N / 2)
#define HI_BIN_HZ    ((float)AUDIO_FS_HZ / HI_N)
#define LO_BIN_HZ    ((float)LO_FS_HZ / LO_N)

#define DB_Q8_MIN    (-127 * 256)
#define TILT_DB_OCT  3.0f         // pink-noise compensation: music reads flat

// log2 of the FFT output power for a full-scale sine at a bin centre:
// window input peaks at 2^14, |X| = A * N/4 for a Hann window.
#define HI_FS_LOG2P  (2 * (14 + HI_LOG2 - 2))
#define LO_FS_LOG2P  (2 * (14 + LO_LOG2 - 2))

#define XBLEND_LO_HZ 380.0f          // columns crossfade bass -> full-rate FFT over this span
#define XBLEND_HI_HZ 600.0f

enum { MAP_INTERP = 0, MAP_MAX = 1 };

typedef struct {
    uint8_t  mode;
    uint16_t a;         // INTERP: bin i (spline spans i..i+1)    MAX: first bin
    int16_t  b;         // INTERP: fraction Q8                    MAX: last bin (inclusive)
    int16_t  gain_q8;   // tilt + density normalisation (+ CIC droop for the bass FFT)
} binmap_t;

typedef struct {
    binmap_t lo, hi;
    uint16_t w_hi;      // 0 = bass FFT only, 256 = full-rate only, between = crossfade
} colmap_t;

static int16_t  _win_hi[HI_N];
static int16_t  _win_lo[LO_N];
static int32_t  _work[HI_N];
static int32_t  _bins[HI_N + 2];
static int16_t  _hi_db[HI_BINS + 1];
static int16_t  _lo_db[LO_BINS + 1];
static colmap_t _map[SPEC_COLS];
static uint8_t  _log2_frac[128];

// ============================================================================
// Frequency helpers
// ============================================================================

float spectrum_col_hz(float c) {
    return SPEC_FMIN_HZ * powf(SPEC_FMAX_HZ / SPEC_FMIN_HZ, c / (SPEC_COLS - 1));
}

float spectrum_hz_col(float f) {
    return (SPEC_COLS - 1) * logf(f / SPEC_FMIN_HZ) / logf(SPEC_FMAX_HZ / SPEC_FMIN_HZ);
}

/** Power gain (dB) that undoes the 4th-order CIC passband droop at f. */
static float cic_comp_db(float f) {
    if (f <= 0.0f) return 0.0f;
    float x = (float)M_PI * f / AUDIO_FS_HZ;
    float h = sinf(x * LO_DECIM) / (LO_DECIM * sinf(x));
    return -80.0f * log10f(fabsf(h));   // 20*log10(h^4)
}

// ============================================================================
// Fixed-point log
// ============================================================================

/** log2(x) in Q8 for x > 0. */
static inline int32_t log2_q8(uint32_t x) {
    int lz = __builtin_clz(x);
    uint32_t norm = x << lz;               // leading 1 at bit 31
    return (31 - lz) * 256 + _log2_frac[(norm >> 24) & 0x7F];
}

/** Bin power to dBFS Q8. */
static inline int16_t bin_db_q8(int32_t re, int32_t im, int32_t log2_offset_q8) {
    uint32_t ur = (uint32_t)(re < 0 ? -re : re);
    uint32_t ui = (uint32_t)(im < 0 ? -im : im);
    uint32_t p = ur * ur + ui * ui;
    if (p == 0) return DB_Q8_MIN;
    int32_t db = ((log2_q8(p) + log2_offset_q8) * 771) >> 8;   // 10*log10(2) = 3.0103 = 771/256
    return (int16_t)(db < DB_Q8_MIN ? DB_Q8_MIN : db);
}

// ============================================================================
// Init
// ============================================================================

void cic_reset(cic_t *cic) {
    memset(cic, 0, sizeof(*cic));
}

static void build_binmap(binmap_t *m, float f, bool lo) {
    const float half_step = powf(SPEC_FMAX_HZ / SPEC_FMIN_HZ, 0.5f / (SPEC_COLS - 1));
    float bin_hz = lo ? LO_BIN_HZ : HI_BIN_HZ;
    int nbins = lo ? LO_BINS : HI_BINS;
    int k0 = (int)ceilf(f / half_step / bin_hz);
    int k1 = (int)floorf(f * half_step / bin_hz);
    if (k1 > nbins) k1 = nbins;
    if (k1 - k0 >= 1) {
        m->mode = MAP_MAX;
        m->a = (uint16_t)k0;
        m->b = (uint16_t)k1;
    } else {
        // Catmull-Rom through bins i-1..i+2: continuous, and a tone between
        // two bins peaks close to its true column.
        float p = f / bin_hz;
        int i = (int)floorf(p);
        if (i < 1) i = 1;
        if (i > nbins - 2) i = nbins - 2;
        m->mode = MAP_INTERP;
        m->a = (uint16_t)i;
        m->b = (int16_t)lroundf((p - i) * 256.0f);
    }
    // Display is normalised to spectral density: broadband noise then reads the
    // same through both FFTs (tones read 6 dB lower above the crossover; the
    // dominant-frequency readout stays calibrated for tones).
    float g = TILT_DB_OCT * log2f(f / 1000.0f) +
              (lo ? cic_comp_db(f) : -10.0f * log10f(HI_BIN_HZ / LO_BIN_HZ));
    m->gain_q8 = (int16_t)lroundf(g * 256.0f);
}

void dsp_init(void) {
    fft_q_init();

    for (int m = 0; m < 128; m++) {
        _log2_frac[m] = (uint8_t)lroundf(256.0f * log2f(1.0f + m / 128.0f));
    }
    // Periodic Hann (better spectral leakage behaviour than symmetric)
    for (int i = 0; i < HI_N; i++) {
        _win_hi[i] = (int16_t)lroundf(32767.0f * 0.5f * (1.0f - cosf(2.0f * (float)M_PI * i / HI_N)));
    }
    for (int i = 0; i < LO_N; i++) {
        _win_lo[i] = (int16_t)lroundf(32767.0f * 0.5f * (1.0f - cosf(2.0f * (float)M_PI * i / LO_N)));
    }

    for (int c = 0; c < SPEC_COLS; c++) {
        float f = spectrum_col_hz((float)c);
        colmap_t *m = &_map[c];
        float w = log2f(f / XBLEND_LO_HZ) / log2f(XBLEND_HI_HZ / XBLEND_LO_HZ);
        m->w_hi = (uint16_t)(w <= 0.0f ? 0 : w >= 1.0f ? 256 : lroundf(w * 256.0f));
        if (m->w_hi < 256) build_binmap(&m->lo, f, true);
        if (m->w_hi > 0) build_binmap(&m->hi, f, false);
    }
}

// ============================================================================
// Analysis
// ============================================================================

/** Parabolic peak refinement on dB samples; returns bin offset in [-0.5, 0.5]. */
static float parabolic(float a, float b, float c, float *peak) {
    float den = a - 2.0f * b + c;
    float d = (den != 0.0f) ? 0.5f * (a - c) / den : 0.0f;
    if (d > 0.5f) d = 0.5f;
    if (d < -0.5f) d = -0.5f;
    *peak = b - 0.25f * (a - c) * d;
    return d;
}

static inline int32_t eval_binmap(const binmap_t *m, const int16_t *db) {
    int32_t v;
    if (m->mode == MAP_MAX) {
        v = db[m->a];
        for (int k = m->a + 1; k <= m->b; k++) if (db[k] > v) v = db[k];
    } else {
        int32_t p0 = db[m->a - 1], p1 = db[m->a], p2 = db[m->a + 1], p3 = db[m->a + 2];
        int32_t t = m->b;
        int32_t c3 = -p0 + 3 * p1 - 3 * p2 + p3;
        int32_t c2 = 2 * p0 - 5 * p1 + 4 * p2 - p3;
        int32_t c1 = p2 - p0;
        v = p1 + ((((((c3 * t) >> 8) + c2) * t >> 8) + c1) * t >> 9);
    }
    return v + m->gain_q8;
}

void RAMFUNC(dsp_analyze)(const uint16_t *hi, const int32_t *lo, dsp_result_t *r) {
    // --- Full-rate FFT ---
    int32_t sum = 0;
    for (int i = 0; i < HI_N; i++) sum += hi[i];
    const int32_t mean = sum >> HI_LOG2;
    for (int i = 0; i < HI_N; i++) {
        _work[i] = (((int32_t)hi[i] - mean) * _win_hi[i]) >> 12;
    }
    int e = fft_q_real(_work, HI_LOG2, _bins);
    int32_t off = 256 * (2 * e - HI_FS_LOG2P);
    for (int k = 0; k <= HI_BINS; k++) {
        _hi_db[k] = bin_db_q8(_bins[2 * k], _bins[2 * k + 1], off);
    }

    // --- Bass FFT ---
    sum = 0;
    for (int i = 0; i < LO_N; i++) sum += lo[i];
    const int32_t lmean = sum >> LO_LOG2;
    for (int i = 0; i < LO_N; i++) {
        _work[i] = (((lo[i] - lmean) >> 1) * _win_lo[i]) >> 15;
    }
    e = fft_q_real(_work, LO_LOG2, _bins);
    off = 256 * (2 * e - LO_FS_LOG2P);
    for (int k = 0; k <= LO_BINS; k++) {
        _lo_db[k] = bin_db_q8(_bins[2 * k], _bins[2 * k + 1], off);
    }

    // --- Columns ---
    for (int c = 0; c < SPEC_COLS; c++) {
        const colmap_t *m = &_map[c];
        int32_t v;
        if (m->w_hi == 0)        v = eval_binmap(&m->lo, _lo_db);
        else if (m->w_hi == 256) v = eval_binmap(&m->hi, _hi_db);
        else {
            int32_t vl = eval_binmap(&m->lo, _lo_db), vh = eval_binmap(&m->hi, _hi_db);
            v = vl + (((vh - vl) * (int32_t)m->w_hi) >> 8);
        }
        r->col_db[c] = (int16_t)(v < DB_Q8_MIN ? DB_Q8_MIN : (v > 32767 ? 32767 : v));
    }

    // --- Dominant frequency (untilted dBFS) ---
    int hk0 = (int)ceilf(XOVER_HZ / HI_BIN_HZ), hk1 = (int)floorf(SPEC_FMAX_HZ / HI_BIN_HZ);
    int hbest = hk0;
    for (int k = hk0 + 1; k <= hk1; k++) if (_hi_db[k] > _hi_db[hbest]) hbest = k;

    int lk0 = (int)ceilf(SPEC_FMIN_HZ / LO_BIN_HZ), lk1 = (int)floorf(XOVER_HZ / LO_BIN_HZ);
    int lbest = lk0;
    for (int k = lk0 + 1; k <= lk1; k++) if (_lo_db[k] > _lo_db[lbest]) lbest = k;

    float pk;
    float lo_level = _lo_db[lbest] / 256.0f + cic_comp_db(lbest * LO_BIN_HZ);
    if (lo_level > _hi_db[hbest] / 256.0f) {
        float d = parabolic(_lo_db[lbest - 1] / 256.0f, _lo_db[lbest] / 256.0f,
                            _lo_db[lbest + 1] / 256.0f, &pk);
        r->peak_hz = (lbest + d) * LO_BIN_HZ;
        r->peak_dbfs = pk + cic_comp_db(r->peak_hz);
    } else {
        float d = parabolic(_hi_db[hbest - 1] / 256.0f, _hi_db[hbest] / 256.0f,
                            _hi_db[hbest + 1] / 256.0f, &pk);
        r->peak_hz = (hbest + d) * HI_BIN_HZ;
        r->peak_dbfs = pk;
    }
}

// ============================================================================
// AGC + ballistics
// ============================================================================

#define RANGE_Q8        ((int32_t)(LEVEL_RANGE_DB * 256))
#define MIN_SPAN_Q8     (30 * 256)     // never zoom tighter than 30 dB
#define HEADROOM_Q8     (3 * 256)
#define REF_MIN_Q8      (-60 * 256)
#define REF_DECAY_Q8    4              // ~3 dB/s at 187.5 hops/s
#define FALL_DB_S       30.0f

// Noise floor: per-column mean of the dB level, following only values near the
// floor (~0.7 s EMA). Anything well above just nudges it up slowly, and a cap
// keeps sustained loud tones from ever being treated as noise.
#define FLOOR_INIT_Q8   (-70 * 256)
#define FLOOR_CAP_Q8    (-35 * 256)
#define FLOOR_TRACK_Q8  (6 * 256)      // values within this of the floor update the mean
#define FLOOR_RISE_Q8   2              // otherwise creep up ~1.5 dB/s
#define GATE_Q8         (8 * 256)      // expander threshold above the floor
#define EXPAND          3              // 1:3 downward expansion below the threshold
#define BOTTOM_OVER_FLOOR_Q8 (3 * 256)

void levels_init(levels_t *lv) {
    memset(lv, 0, sizeof(*lv));
    lv->ref_q8 = REF_MIN_Q8;
    lv->agc = true;
    lv->fixed_top_q8 = -6 * 256;
    for (int c = 0; c < SPEC_COLS; c++) lv->floor_q8[c] = FLOOR_INIT_Q8;
}

void RAMFUNC(levels_update)(levels_t *lv, const int16_t *col_db, uint8_t *raw,
                            float *out_top_dbfs, float *out_range_db) {
    static int32_t ex[SPEC_COLS];
    int32_t m = DB_Q8_MIN, fsum = 0;

    for (int c = 0; c < SPEC_COLS; c++) {
        int32_t d = col_db[c], f = lv->floor_q8[c];
        f = (d < f + FLOOR_TRACK_Q8) ? f + ((d - f) >> 7) : f + FLOOR_RISE_Q8;
        if (f > FLOOR_CAP_Q8) f = FLOOR_CAP_Q8;
        lv->floor_q8[c] = f;
        fsum += f;
        int32_t t = f + GATE_Q8;
        if (d < t) {
            d = t - EXPAND * (t - d);
            if (d < DB_Q8_MIN) d = DB_Q8_MIN;
        }
        ex[c] = d;
        if (d > m) m = d;
    }

    if (m > lv->ref_q8) lv->ref_q8 += (m - lv->ref_q8) >> 2;
    else                lv->ref_q8 -= REF_DECAY_Q8;
    if (lv->ref_q8 < REF_MIN_Q8) lv->ref_q8 = REF_MIN_Q8;
    if (lv->ref_q8 > 0) lv->ref_q8 = 0;

    int32_t top, bottom;
    if (lv->agc) {
        top = lv->ref_q8 + HEADROOM_Q8;
        bottom = top - RANGE_Q8;
        int32_t noise = fsum / SPEC_COLS + BOTTOM_OVER_FLOOR_Q8;
        if (bottom < noise) bottom = noise;          // silence looks silent
        if (top - bottom < MIN_SPAN_Q8) top = bottom + MIN_SPAN_Q8;
    } else {
        top = lv->fixed_top_q8;
        bottom = top - RANGE_Q8;
    }
    const int32_t span = top - bottom;
    const int32_t scale_q8 = (int32_t)(((int64_t)LEVEL_MAX << 8) / span);
    const int32_t fall = (int32_t)(LEVEL_MAX * 256.0f * FALL_DB_S * HOP_SAMPLES / AUDIO_FS_HZ / span);
    *out_top_dbfs = top / 256.0f;
    *out_range_db = span / 256.0f;

    for (int c = 0; c < SPEC_COLS; c++) {
        int32_t d = ex[c] - bottom;
        int32_t level = d <= 0 ? 0 : (d * scale_q8) >> 8;
        if (level > (int32_t)LEVEL_MAX) level = LEVEL_MAX;
        if (raw) raw[c] = (uint8_t)(level >> 8);

        int32_t b = lv->bal[c];
        if (level > b) b = level;
        else {
            b -= fall;
            if (b < level) b = level;
        }
        lv->bal[c] = (uint16_t)b;
    }
}
