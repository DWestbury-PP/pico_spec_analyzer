/**
 * @file platform.h
 * @brief Small portability shims so the DSP core also builds on the host for tests.
 */

#ifndef PLATFORM_H
#define PLATFORM_H

#ifdef HOST_TEST
    #define RAMFUNC(name) name
#else
    #include "pico.h"
    // Execute from SRAM: avoids XIP cache misses in the hot loops.
    #define RAMFUNC(name) __not_in_flash_func(name)
#endif

#endif // PLATFORM_H
