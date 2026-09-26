/**
 * @file main.c
 * @brief Boot: overclock, bring up peripherals, split work across both cores.
 *
 *   Core 0: ADC DMA capture + CIC + dual-resolution FFT + AGC/ballistics
 *   Core 1: themes, overlays, touch/serial input, DMA streaming to the panel
 */

#include "config.h"
#include "audio/audio_task.h"
#include "core/crash.h"
#include "core/shared.h"
#include "display/gfx.h"
#include "display/ili9341.h"
#include "display/renderer.h"
#include "touch/xpt2046.h"
#include "hardware/clocks.h"
#include "hardware/vreg.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"

int main(void) {
    crash_init();

    vreg_set_voltage(SYS_VREG_VOLTAGE);
    sleep_ms(10);
    set_sys_clock_khz(SYS_CLOCK_KHZ, true);

    stdio_init_all();
    shared_init();

    ili9341_init();
    ili9341_set_rotation(DISPLAY_ROTATION);
    ili9341_fill_screen(0x0000);

    g_boot_info.sys_hz = clock_get_hz(clk_sys);
    g_boot_info.spi_hz = ili9341_stream_hz();
    g_boot_info.pixfmt = ili9341_read_reg(0x0C, 1);
    g_boot_info.selftest_bad = ili9341_selftest();

    xpt2046_init();
    gfx_init();
    audio_task_init();

    multicore_launch_core1(renderer_run);
    audio_task_run();
}
