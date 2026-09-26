/**
 * @file crash.h
 * @brief Post-mortem crash capture and self-recovery.
 *
 * - HardFault on either core: PC/LR/core are saved to watchdog scratch
 *   registers (they survive reset), then the chip reboots.
 * - Hang: the watchdog fires unless the render loop keeps feeding it while
 *   the audio core is still producing hops.
 * - Crash loop: three crash reboots without a healthy run in between drop
 *   into BOOTSEL, so the board can always be re-flashed over USB.
 */

#ifndef CRASH_H
#define CRASH_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool     fault;        // previous boot ended in a HardFault
    bool     hang;         // previous boot ended in a watchdog timeout
    uint32_t pc, lr, core;
    uint32_t streak;       // consecutive crash reboots
} crash_info_t;

extern crash_info_t g_crash;

/** Call first thing in main(). May not return (enters BOOTSEL on a crash loop). */
void crash_init(void);

/** Arm the watchdog. Call once both cores are running. */
void crash_watchdog_start(void);

/** Feed from the render loop; pass the audio core's latest hop number. */
void crash_heartbeat(uint32_t audio_hop);

#endif // CRASH_H
