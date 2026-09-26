/**
 * @file shared.h
 * @brief Lock-free hand-off between the audio core (0) and the render core (1).
 *
 * - Mailbox: seqlock around the latest spectrum_frame_t. The writer never
 *   blocks; the reader retries if it raced a write.
 * - Hop queue: single-producer/single-consumer ring of every per-hop spectrum,
 *   for themes that must not miss a hop (waterfall, terrain).
 * - Controls: single-word flags written by core 1, read by core 0.
 */

#ifndef SHARED_H
#define SHARED_H

#include <stdbool.h>
#include <stdint.h>
#include "audio/spectrum.h"

#define HOPQ_LEN 16   // 85 ms of hops; the render loop drains every frame

typedef enum {
    TEST_OFF = 0,
    TEST_440,
    TEST_1K,
    TEST_SWEEP,
    TEST_CHORD,
    TEST_COUNT
} test_signal_t;

typedef struct {
    volatile uint8_t test_signal;   // test_signal_t: replaces the mic input when non-zero
    volatile bool    agc;
} audio_ctl_t;

extern audio_ctl_t g_audio_ctl;

void shared_init(void);

// Mailbox
void mailbox_publish(const spectrum_frame_t *f);
/** Copy the latest frame if it is newer than *seen. Returns true on a new frame. */
bool mailbox_read(spectrum_frame_t *dst, uint32_t *seen);

// Hop queue
spectrum_hop_t *hopq_write_slot(void);   // NULL if full (drop is counted)
void hopq_commit(void);
const spectrum_hop_t *hopq_peek(void);   // NULL if empty
void hopq_pop(void);
uint32_t hopq_drops(void);

#endif // SHARED_H
