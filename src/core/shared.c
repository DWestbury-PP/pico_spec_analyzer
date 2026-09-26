/**
 * @file shared.c
 * @brief Seqlock mailbox and SPSC hop queue between cores.
 */

#include "core/shared.h"
#include "hardware/sync.h"
#include <string.h>

audio_ctl_t g_audio_ctl;

static spectrum_frame_t _mb;
static volatile uint32_t _mb_seq;

static spectrum_hop_t _hopq[HOPQ_LEN];
static volatile uint32_t _hop_head;   // written by core 0
static volatile uint32_t _hop_tail;   // written by core 1
static volatile uint32_t _hop_drops;

void shared_init(void) {
    memset(&_mb, 0, sizeof(_mb));
    _mb_seq = 0;
    _hop_head = _hop_tail = _hop_drops = 0;
    g_audio_ctl.test_signal = TEST_OFF;
    g_audio_ctl.agc = true;
}

void mailbox_publish(const spectrum_frame_t *f) {
    _mb_seq++;              // odd: write in progress
    __dmb();
    memcpy(&_mb, f, sizeof(_mb));
    __dmb();
    _mb_seq++;              // even: stable
}

bool mailbox_read(spectrum_frame_t *dst, uint32_t *seen) {
    uint32_t s1, s2;
    do {
        s1 = _mb_seq;
        if (s1 == *seen) return false;
        if (s1 & 1) continue;
        __dmb();
        memcpy(dst, &_mb, sizeof(*dst));
        __dmb();
        s2 = _mb_seq;
    } while ((s1 & 1) || s1 != s2);
    *seen = s1;
    return true;
}

spectrum_hop_t *hopq_write_slot(void) {
    if (_hop_head - _hop_tail >= HOPQ_LEN) {
        _hop_drops++;
        return NULL;
    }
    return &_hopq[_hop_head % HOPQ_LEN];
}

void hopq_commit(void) {
    __dmb();
    _hop_head++;
}

const spectrum_hop_t *hopq_peek(void) {
    if (_hop_head == _hop_tail) return NULL;
    __dmb();
    return &_hopq[_hop_tail % HOPQ_LEN];
}

void hopq_pop(void) {
    __dmb();
    _hop_tail++;
}

uint32_t hopq_drops(void) {
    return _hop_drops;
}
