/**
 * @file dsp.h
 * @brief Portable analysis core: CIC decimator, dual-resolution spectrum,
 *        log-frequency column mapping, AGC and ballistics.
 *
 * No SDK dependencies, so it is unit-tested on the host (tests/test_dsp.c).
 */

#ifndef DSP_H
#define DSP_H

#include <stdint.h>
#include <stdbool.h>
#include "audio/spectrum.h"

// --- 4th-order CIC, R = LO_DECIM --------------------------------------------
typedef struct {
    uint32_t integ[4];   // modular arithmetic: wrap-around is intended
    uint32_t comb[4];
    uint32_t phase;
} cic_t;

void cic_reset(cic_t *cic);

/** Push one DC-centred sample; returns true and writes *out every LO_DECIM inputs. */
static inline bool cic_push(cic_t *cic, int32_t x, int32_t *out) {
    uint32_t v = (uint32_t)x;
    v = cic->integ[0] += v;
    v = cic->integ[1] += v;
    v = cic->integ[2] += v;
    v = cic->integ[3] += v;
    if (++cic->phase < LO_DECIM) return false;
    cic->phase = 0;
    for (int i = 0; i < 4; i++) {
        uint32_t d = v - cic->comb[i];
        cic->comb[i] = v;
        v = d;
    }
    // Gain is R^4 = 2^16; scale to 16x ADC counts (headroom for the window step).
    *out = (int32_t)v >> 12;
    return true;
}

// --- Analysis ---------------------------------------------------------------
typedef struct {
    int16_t col_db[SPEC_COLS];   // dBFS in Q8, tilt and CIC droop applied
    float   peak_hz;
    float   peak_dbfs;
} dsp_result_t;

void dsp_init(void);

/**
 * @param hi  HI_N raw ADC samples (12-bit), oldest first
 * @param lo  LO_N CIC outputs, oldest first
 */
void dsp_analyze(const uint16_t *hi, const int32_t *lo, dsp_result_t *r);

// --- AGC + ballistics -------------------------------------------------------
typedef struct {
    int32_t  ref_q8;             // tracked loudest column, dBFS Q8
    bool     agc;                // false: fixed reference at fixed_top_q8
    int32_t  fixed_top_q8;
    int32_t  floor_q8[SPEC_COLS];    // tracked noise floor per column
    uint16_t bal[SPEC_COLS];
} levels_t;

void levels_init(levels_t *lv);

/**
 * Convert one hop of column dB to display levels: per-column noise-floor
 * expander, AGC with a noise-tracking bottom, instant attack / timed release.
 * @param raw  optional (may be NULL): per-hop level 0..255 (no ballistics)
 * @param out_top_dbfs  receives the dBFS mapped to LEVEL_MAX
 * @param out_range_db  receives the dB span of 0..LEVEL_MAX
 */
void levels_update(levels_t *lv, const int16_t *col_db, uint8_t *raw,
                   float *out_top_dbfs, float *out_range_db);

#define LEVEL_RANGE_DB   54.0f

#endif // DSP_H
