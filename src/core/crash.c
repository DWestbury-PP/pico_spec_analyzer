/**
 * @file crash.c
 * @brief HardFault capture, watchdog and crash-loop escape to BOOTSEL.
 */

#include "core/crash.h"
#include "platform.h"
#include "hardware/exception.h"
#include "hardware/watchdog.h"
#include "pico/bootrom.h"
#include "pico/time.h"
#include "hardware/sync.h"

#define FAULT_MAGIC     0xFA017ED0u
#define WATCHDOG_MS     2000
#define HEALTHY_US      10000000ull     // run this long and the crash streak resets
#define AUDIO_STALL_US  1000000ull
#define LOOP_LIMIT      3

// scratch[0..3] belong to the application; watchdog_reboot() and the bootrom use 4..7
#define S_MAGIC  0
#define S_PC     1
#define S_LR     2
#define S_STREAK 3

crash_info_t g_crash;

static uint64_t _boot_us;
static uint64_t _last_hop_change_us;
static uint32_t _last_hop;
static bool _healthy_marked;

void __attribute__((used)) RAMFUNC(crash_record)(uint32_t *frame) {
    uint32_t core = *(volatile uint32_t *)(SIO_BASE + SIO_CPUID_OFFSET);
    watchdog_hw->scratch[S_MAGIC] = FAULT_MAGIC ^ core;
    watchdog_hw->scratch[S_PC] = frame[6];
    watchdog_hw->scratch[S_LR] = frame[5];
    watchdog_reboot(0, 0, 0);
    while (true) __asm volatile("nop");
}

/** Pick the stack that holds the exception frame, then hand it to crash_record. */
static void __attribute__((naked)) RAMFUNC(hardfault_entry)(void) {
    __asm volatile(
        "movs r0, #4        \n"
        "mov  r1, lr        \n"
        "tst  r0, r1        \n"
        "beq  1f            \n"
        "mrs  r0, psp       \n"
        "b    2f            \n"
        "1: mrs r0, msp     \n"
        "2: ldr r2, =crash_record \n"
        "bx   r2            \n");
}

void crash_init(void) {
    const uint32_t magic = watchdog_hw->scratch[S_MAGIC];
    uint32_t streak = watchdog_hw->scratch[S_STREAK];

    if (watchdog_caused_reboot() && (magic & ~1u) == FAULT_MAGIC) {
        g_crash.fault = true;
        g_crash.core = magic & 1u;
        g_crash.pc = watchdog_hw->scratch[S_PC];
        g_crash.lr = watchdog_hw->scratch[S_LR];
    } else if (watchdog_enable_caused_reboot()) {
        g_crash.hang = true;      // armed watchdog expired (not a forced/picotool reboot)
    }
    watchdog_hw->scratch[S_MAGIC] = 0;

    if (g_crash.fault || g_crash.hang) streak = (streak > 100 ? 0 : streak) + 1;
    else streak = 0;
    watchdog_hw->scratch[S_STREAK] = streak;
    g_crash.streak = streak;

    if (streak >= LOOP_LIMIT) {
        watchdog_hw->scratch[S_STREAK] = 0;
        reset_usb_boot(0, 0);     // crash loop: park in BOOTSEL for re-flashing
    }

    exception_set_exclusive_handler(HARDFAULT_EXCEPTION, hardfault_entry);
    _boot_us = time_us_64();
}

void crash_watchdog_start(void) {
    _last_hop_change_us = time_us_64();
    watchdog_enable(WATCHDOG_MS, true);
}

void crash_heartbeat(uint32_t audio_hop) {
    uint64_t now = time_us_64();
    if (audio_hop != _last_hop) {
        _last_hop = audio_hop;
        _last_hop_change_us = now;
    }
    if (now - _last_hop_change_us < AUDIO_STALL_US) watchdog_update();
    if (!_healthy_marked && now - _boot_us > HEALTHY_US) {
        watchdog_hw->scratch[S_STREAK] = 0;
        _healthy_marked = true;
    }
}
