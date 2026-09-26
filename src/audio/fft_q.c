/**
 * @file fft_q.c
 * @brief Block-floating-point radix-2 FFT with real-input packing.
 *
 * An n-point real FFT runs as an n/2-point complex FFT on z[m] = x[2m] + j x[2m+1],
 * followed by the standard split step. That halves the butterfly count.
 */

#include "audio/fft_q.h"
#include "platform.h"
#include <math.h>

#define TW_N     FFT_Q_MAX_N          // twiddles are W_1024^k
#define REV_BITS (FFT_Q_MAX_LOG2 - 1) // bit-reverse table covers the 512-point complex FFT
#define RND      (1 << 14)            // round-to-nearest for Q15 products

static int16_t _cos[TW_N / 2];
static int16_t _sin[TW_N / 2];
static uint16_t _rev[1 << REV_BITS];

void fft_q_init(void) {
    for (int k = 0; k < TW_N / 2; k++) {
        double a = 2.0 * M_PI * k / TW_N;
        _cos[k] = (int16_t)lround(32767.0 * cos(a));
        _sin[k] = (int16_t)lround(32767.0 * sin(a));
    }
    for (int i = 0; i < (1 << REV_BITS); i++) {
        uint32_t r = 0;
        for (int b = 0; b < REV_BITS; b++) r |= ((i >> b) & 1u) << (REV_BITS - 1 - b);
        _rev[i] = (uint16_t)r;
    }
}

static inline uint32_t iabs(int32_t v) { return (uint32_t)(v < 0 ? -v : v); }

/** In-place complex FFT of m = 2^log2m points (interleaved re,im). Returns shifts applied. */
static int RAMFUNC(fft_complex)(int32_t *z, int log2m) {
    const int m = 1 << log2m;
    const int rshift = REV_BITS - log2m;
    uint32_t mag = 0;

    for (int i = 0; i < m; i++) {
        int j = _rev[i] >> rshift;
        if (j > i) {
            int32_t tr = z[2 * i], ti = z[2 * i + 1];
            z[2 * i] = z[2 * j];     z[2 * i + 1] = z[2 * j + 1];
            z[2 * j] = tr;           z[2 * j + 1] = ti;
        }
        mag |= iabs(z[2 * i]) | iabs(z[2 * i + 1]);
    }

    int exp = 0;
    for (int len = 2, tstep = TW_N / 2; len <= m; len <<= 1, tstep >>= 1) {
        const int sh = (mag >= (1u << 14)) ? 1 : 0;   // OR-of-abs >= 2^14 <=> max >= 2^14
        const int half = len >> 1;
        exp += sh;
        mag = 0;
        for (int j = 0; j < half; j++) {
            const int32_t c = _cos[j * tstep];
            const int32_t s = _sin[j * tstep];
            for (int i = j; i < m; i += len) {
                int32_t *a = &z[2 * i];
                int32_t *b = &z[2 * (i + half)];
                // t = b * W, W = c - js
                int32_t tr = ((b[0] * c + RND) >> 15) + ((b[1] * s + RND) >> 15);
                int32_t ti = ((b[1] * c + RND) >> 15) - ((b[0] * s + RND) >> 15);
                int32_t ar = a[0] + sh, ai = a[1] + sh;   // round the conditional halving
                int32_t r0 = (ar + tr) >> sh, i0 = (ai + ti) >> sh;
                int32_t r1 = (ar - tr) >> sh, i1 = (ai - ti) >> sh;
                a[0] = r0; a[1] = i0; b[0] = r1; b[1] = i1;
                mag |= iabs(r0) | iabs(i0) | iabs(r1) | iabs(i1);
            }
        }
    }
    return exp;
}

int RAMFUNC(fft_q_real)(int32_t *buf, int log2n, int32_t *out) {
    const int n = 1 << log2n;
    const int m = n >> 1;
    const int tstep = TW_N / n;

    // Input layout x[0..n-1] already equals interleaved z[m] = x[2m] + j x[2m+1].
    int e = fft_complex(buf, log2n - 1);

    // Split: X[k] = (Fe + W^k Fo) / 2, with the /2 folded into the exponent.
    const int32_t z0r = buf[0], z0i = buf[1];
    out[0] = (z0r + z0i) >> 1;       out[1] = 0;
    out[2 * m] = (z0r - z0i) >> 1;   out[2 * m + 1] = 0;

    for (int k = 1; k < m; k++) {
        int32_t ar = buf[2 * k],        ai = buf[2 * k + 1];
        int32_t br = buf[2 * (m - k)],  bi = -buf[2 * (m - k) + 1];   // conj(Z[m-k])
        int32_t fer = (ar + br) >> 1,   fei = (ai + bi) >> 1;
        int32_t forr = (ai - bi) >> 1,  foi = (br - ar) >> 1;
        int32_t c = _cos[k * tstep], s = _sin[k * tstep];
        int32_t wr = ((forr * c + RND) >> 15) + ((foi * s + RND) >> 15);
        int32_t wi = ((foi * c + RND) >> 15) - ((forr * s + RND) >> 15);
        out[2 * k]     = (fer + wr + 1) >> 1;
        out[2 * k + 1] = (fei + wi + 1) >> 1;
    }
    // Fe and Fo are exact; the final >>1 keeps |X| < 2^15.5 and costs one exponent step.
    return e + 1;
}
