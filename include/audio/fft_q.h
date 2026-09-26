/**
 * @file fft_q.h
 * @brief Block-floating-point fixed-point real FFT for Cortex-M0+ (no FPU).
 *
 * Data is int32 with Q15 twiddles. Each stage shifts right by one only when
 * the running magnitude could overflow, so quiet signals keep full precision
 * and the returned block exponent restores absolute scale.
 */

#ifndef FFT_Q_H
#define FFT_Q_H

#include <stdint.h>

#define FFT_Q_MAX_LOG2  10                  // largest real FFT: 1024 points
#define FFT_Q_MAX_N     (1 << FFT_Q_MAX_LOG2)

/** Build twiddle and bit-reverse tables. Call once. */
void fft_q_init(void);

/**
 * @brief Real FFT of n = 2^log2n samples.
 *
 * @param buf  n input samples, |x| < 2^15. Destroyed (used as workspace).
 * @param log2n  2..FFT_Q_MAX_LOG2
 * @param out  receives n/2+1 complex bins as interleaved re,im (n+2 int32)
 * @return block exponent e: true DFT X[k] = out[k] * 2^e. Output components
 *         are < 2^15.5, so re*re + im*im fits in uint32.
 */
int fft_q_real(int32_t *buf, int log2n, int32_t *out);

#endif // FFT_Q_H
