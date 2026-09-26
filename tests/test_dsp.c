/**
 * @file test_dsp.c
 * @brief Host-side verification of the fixed-point DSP core.
 *
 * Build and run: ./tests/run_host_tests.sh
 */

#include "audio/dsp.h"
#include "audio/fft_q.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int _failures = 0;

#define CHECK(cond, ...) do { \
    if (!(cond)) { _failures++; printf("  FAIL: "); printf(__VA_ARGS__); printf("\n"); } \
} while (0)

// Deterministic noise
static uint32_t _rng = 12345;
static double urand(void) { _rng = _rng * 1664525u + 1013904223u; return (_rng >> 8) / 16777216.0; }
static double gauss(void) { double u = urand() + 1e-12, v = urand(); return sqrt(-2 * log(u)) * cos(2 * M_PI * v); }

/** Synthesise ADC capture: sine of amplitude amp (counts) + gaussian noise (counts rms). */
static void make_adc(uint16_t *dst, int n, int start, double f, double amp, double noise) {
    for (int i = 0; i < n; i++) {
        double t = (double)(start + i) / AUDIO_FS_HZ;
        double v = 2048.0 + amp * sin(2 * M_PI * f * t + 0.3) + noise * gauss();
        long q = lround(v);
        dst[i] = (uint16_t)(q < 0 ? 0 : q > 4095 ? 4095 : q);
    }
}

// ---------------------------------------------------------------------------
static void test_fft_vs_dft(void) {
    printf("FFT vs double DFT (1024 and 256 pt, random + tone input)\n");
    for (int log2n = 8; log2n <= 10; log2n += 2) {
        int n = 1 << log2n;
        static int32_t buf[1024], out[1026];
        static double x[1024];
        for (int i = 0; i < n; i++) {
            x[i] = 12000.0 * sin(2 * M_PI * 37.3 * i / n) + 900.0 * gauss();
            buf[i] = (int32_t)lround(x[i]);
            x[i] = buf[i];
        }
        int e = fft_q_real(buf, log2n, out);
        double worst = 0, ref_peak = 0;
        static double mag[513];
        for (int k = 0; k <= n / 2; k++) {
            double re = 0, im = 0;
            for (int i = 0; i < n; i++) {
                re += x[i] * cos(2 * M_PI * k * i / n);
                im -= x[i] * sin(2 * M_PI * k * i / n);
            }
            mag[k] = hypot(re, im);
            if (mag[k] > ref_peak) ref_peak = mag[k];
        }
        for (int k = 0; k <= n / 2; k++) {
            double got = hypot(out[2 * k], out[2 * k + 1]) * pow(2, e);
            double err = fabs(got - mag[k]) / ref_peak;   // error relative to peak
            if (err > worst) worst = err;
        }
        double err_db = 20 * log10(worst + 1e-30);
        printf("  n=%4d exp=%d worst bin error = %.1f dB below peak\n", n, e, err_db);
        CHECK(err_db < -70.0, "fixed-point FFT error floor too high (%.1f dB)", err_db);
    }
}

// ---------------------------------------------------------------------------
/** Run the full pipeline (CIC + analysis) on a continuous synthetic stream. */
static void run_stream(double f, double amp, double noise, dsp_result_t *r) {
    static uint16_t hist[48000];
    static int32_t lo_hist[4096];
    int total = 24000;             // 0.5 s, long enough to fill the bass window
    make_adc(hist, total, 0, f, amp, noise);
    cic_t cic;
    cic_reset(&cic);
    int nlo = 0;
    for (int i = 0; i < total; i++) {
        int32_t o;
        if (cic_push(&cic, (int32_t)hist[i] - 2048, &o)) lo_hist[nlo++] = o;
    }
    dsp_analyze(&hist[total - HI_N], &lo_hist[nlo - LO_N], r);
}

static void test_calibration_and_peak(void) {
    printf("Tone calibration: dominant frequency and level\n");
    const double freqs[] = { 41.2, 60.0, 110.0, 220.0, 440.0, 1000.0, 1031.25, 2500.0, 7000.0, 12000.0 };
    const double levels_dbfs[] = { 0.0, -20.0, -40.0 };
    for (size_t li = 0; li < sizeof(levels_dbfs) / sizeof(levels_dbfs[0]); li++) {
        for (size_t fi = 0; fi < sizeof(freqs) / sizeof(freqs[0]); fi++) {
            double amp = 2047.0 * pow(10, levels_dbfs[li] / 20);
            dsp_result_t r;
            run_stream(freqs[fi], amp, 0.5, &r);
            double ferr = 100.0 * fabs(r.peak_hz - freqs[fi]) / freqs[fi];
            double lerr = r.peak_dbfs - levels_dbfs[li];
            int col = 0;
            for (int c = 1; c < SPEC_COLS; c++) if (r.col_db[c] > r.col_db[col]) col = c;
            float want_col = spectrum_hz_col((float)freqs[fi]);
            printf("  %7.2f Hz @ %5.1f dBFS -> %8.2f Hz (%5.2f%%)  %6.2f dBFS (%+5.2f)  col %3d (want %5.1f)\n",
                   freqs[fi], levels_dbfs[li], r.peak_hz, ferr, r.peak_dbfs, lerr, col, want_col);
            CHECK(ferr < (freqs[fi] < 100 ? 3.0 : 1.0), "frequency error %.2f%% at %.1f Hz", ferr, freqs[fi]);
            CHECK(fabs(lerr) < 1.0, "level error %+.2f dB at %.1f Hz", lerr, freqs[fi]);
            CHECK(fabs(col - want_col) <= 2.0f, "column %d vs %.1f at %.1f Hz", col, want_col, freqs[fi]);
        }
    }
}

// ---------------------------------------------------------------------------
static void test_noise_floor(void) {
    printf("Noise floor with 1.5 LSB rms ADC noise (tilted column dBFS)\n");
    dsp_result_t r;
    run_stream(1000.0, 0.0, 1.5, &r);
    int16_t sorted[SPEC_COLS];
    memcpy(sorted, r.col_db, sizeof(sorted));
    for (int i = 1; i < SPEC_COLS; i++)
        for (int j = i; j > 0 && sorted[j - 1] > sorted[j]; j--) { int16_t t = sorted[j]; sorted[j] = sorted[j - 1]; sorted[j - 1] = t; }
    printf("  median %.1f dBFS, max %.1f dBFS\n", sorted[SPEC_COLS / 2] / 256.0, sorted[SPEC_COLS - 1] / 256.0);
}

// ---------------------------------------------------------------------------
static void test_levels(void) {
    printf("AGC + ballistics\n");
    levels_t lv;
    levels_init(&lv);
    int16_t db[SPEC_COLS];
    uint8_t raw[SPEC_COLS];
    float top, range;
    for (int c = 0; c < SPEC_COLS; c++) db[c] = (int16_t)(-70 * 256);
    db[100] = (int16_t)(-12 * 256);
    for (int h = 0; h < 200; h++) levels_update(&lv, db, raw, &top, &range);
    printf("  steady tone at -12 dBFS: top=%.1f dBFS, level=%u raw=%u\n", top, lv.bal[100], raw[100]);
    CHECK(fabsf(top - (-9.0f)) < 0.5f, "AGC top %.2f, expected -9", top);
    CHECK(lv.bal[100] > 60000, "tone column should sit near the top");
    db[100] = (int16_t)(-70 * 256);
    levels_update(&lv, db, raw, &top, &range);
    uint16_t after1 = lv.bal[100];
    for (int h = 0; h < 187; h++) levels_update(&lv, db, raw, &top, &range);
    printf("  release: after 1 hop %u, after 1 s %u\n", after1, lv.bal[100]);
    CHECK(after1 > 60000, "instant attack / timed release violated");
    CHECK(lv.bal[100] < 30000, "release too slow");
}

// ---------------------------------------------------------------------------
static void test_crossover_and_gate(void) {
    printf("White noise: crossover continuity, and noise gating\n");
    dsp_result_t r;
    double below = 0, above = 0;
    int nb = 0, na = 0;
    for (int trial = 0; trial < 20; trial++) {
        run_stream(1000.0, 0.0, 40.0, &r);   // broadband: 40 LSB rms
        for (int c = 0; c < SPEC_COLS; c++) {
            float f = spectrum_col_hz((float)c);
            // remove the display tilt so white noise should read flat
            double d = r.col_db[c] / 256.0 - 3.0 * log2(f / 1000.0);
            if (f > 250 && f < 380) { below += d; nb++; }
            if (f > 600 && f < 900) { above += d; na++; }
        }
    }
    below /= nb; above /= na;
    printf("  mean density below crossover %.1f dB, above %.1f dB (step %+.1f)\n", below, above, above - below);
    CHECK(fabs(above - below) < 3.0, "crossover step %.1f dB", above - below);

    levels_t lv;
    levels_init(&lv);
    uint8_t raw[SPEC_COLS];
    float top, range;
    double mean_noise = 0;
    for (int h = 0; h < 1500; h++) {           // ~8 s of steady noise
        run_stream(1000.0, 0.0, 40.0, &r);
        levels_update(&lv, r.col_db, raw, &top, &range);
        if (h >= 1300) for (int c = 0; c < SPEC_COLS; c++) mean_noise += raw[c];
    }
    mean_noise /= 200.0 * SPEC_COLS;
    printf("  steady noise after 8 s: mean raw level %.1f/255, top %.1f dBFS, span %.1f dB\n", mean_noise, top, range);
    CHECK(mean_noise < 26.0, "noise not gated (mean level %.1f)", mean_noise);

    run_stream(2000.0, 300.0, 40.0, &r);       // tone appears over the noise
    for (int h = 0; h < 20; h++) levels_update(&lv, r.col_db, raw, &top, &range);
    int col = (int)lroundf(spectrum_hz_col(2000.0f));
    printf("  2 kHz tone over noise: raw level %u/255\n", raw[col]);
    CHECK(raw[col] > 200, "tone should reach the top of the display");
}

// ---------------------------------------------------------------------------
static void bench(void) {
    static uint16_t hi[HI_N];
    static int32_t lo[LO_N];
    make_adc(hi, HI_N, 0, 1000, 500, 2);
    for (int i = 0; i < LO_N; i++) lo[i] = (int32_t)(8000 * sin(i * 0.3));
    dsp_result_t r;
    clock_t t0 = clock();
    int iters = 20000;
    for (int i = 0; i < iters; i++) dsp_analyze(hi, lo, &r);
    double us = 1e6 * (double)(clock() - t0) / CLOCKS_PER_SEC / iters;
    printf("Host bench: dsp_analyze %.1f us/hop (device figure comes from the serial stats)\n", us);
}

int main(void) {
    dsp_init();
    test_fft_vs_dft();
    test_calibration_and_peak();
    test_noise_floor();
    test_levels();
    test_crossover_and_gate();
    bench();
    printf(_failures ? "\n%d FAILURE(S)\n" : "\nALL PASS\n", _failures);
    return _failures ? 1 : 0;
}
