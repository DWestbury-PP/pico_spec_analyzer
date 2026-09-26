/**
 * @file spectrum.h
 * @brief Spectrum data contract between the audio core (0) and the render core (1).
 *
 * The spectrum is always SPEC_COLS log-spaced frequency points, one per pixel
 * column of the landscape display, so themes never resample in the hot path.
 */

#ifndef SPECTRUM_H
#define SPECTRUM_H

#include <stdint.h>

// --- Analysis geometry -------------------------------------------------------
#define AUDIO_FS_HZ      48000   // ADC free-runs at exactly 48 MHz / 1000
#define HOP_SAMPLES      256     // new analysis every 5.33 ms (187.5 Hz)

#define HI_LOG2          10      // full-rate FFT: 1024 pts, 46.9 Hz bins, 21 ms window
#define HI_N             (1 << HI_LOG2)
#define LO_DECIM         16      // CIC decimate to 3 kHz for the bass FFT
#define LO_FS_HZ         (AUDIO_FS_HZ / LO_DECIM)
#define LO_LOG2          8       // bass FFT: 256 pts @ 3 kHz, 11.7 Hz bins, 85 ms window
#define LO_N             (1 << LO_LOG2)
#define XOVER_HZ         480.0f  // columns below this read the bass FFT

#define SPEC_COLS        320
#define SPEC_FMIN_HZ     30.0f
#define SPEC_FMAX_HZ     16000.0f

#define SCOPE_N          320     // trigger-aligned waveform samples per frame

#define LEVEL_MAX        65535u  // top of the display range

// --- Per-hop raw spectrum (queued, every hop) --------------------------------
typedef struct {
    uint32_t hop;                // monotonically increasing hop number
    uint64_t t_newest_us;        // capture time of the newest sample analysed
    uint8_t  level[SPEC_COLS];   // raw level 0..255, no ballistics
} spectrum_hop_t;

// --- Latest ballistic spectrum (mailbox, overwritten each hop) ---------------
typedef struct {
    uint32_t hop;
    uint64_t t_newest_us;
    uint16_t level[SPEC_COLS];   // instant attack, timed release; 0..LEVEL_MAX
    int16_t  wave[SCOPE_N];      // rising-edge triggered waveform, ADC counts minus DC
    float    top_dbfs;           // dBFS at LEVEL_MAX (AGC reference + headroom)
    float    range_db;           // dB spanned by 0..LEVEL_MAX
    float    peak_hz;            // dominant frequency (parabolic-interpolated)
    float    peak_dbfs;
    uint16_t bass;               // mean ballistic level below 150 Hz
    uint16_t loud;               // mean ballistic level, all columns
    // Audio-core health
    uint16_t dsp_us_avg;
    uint16_t dsp_us_max;
    uint32_t hops_skipped;       // analysis hops skipped because DSP fell behind
    uint32_t queue_drops;        // hops the render core did not drain in time
} spectrum_frame_t;

/** Centre frequency of display column c (0..SPEC_COLS-1). */
float spectrum_col_hz(float c);

/** Fractional display column for frequency f. */
float spectrum_hz_col(float f);

#endif // SPECTRUM_H
