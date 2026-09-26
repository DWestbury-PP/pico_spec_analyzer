# CMakeLists.txt for Pico Spectrum Analyzer
cmake_minimum_required(VERSION 3.13)

# Include Pico SDK CMake functions
include(pico_sdk_import.cmake)

# Project configuration
project(pico_spec_analyzer C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialize Pico SDK
pico_sdk_init()

# ============================================================================
# Compiler Options
# ============================================================================

# Optimization flags
add_compile_options(-O2)          # Optimize for performance
add_compile_options(-Wall)        # Enable all warnings
add_compile_options(-Wextra)      # Extra warnings
# add_compile_options(-Werror)    # Treat warnings as errors (uncomment when ready)

# Debug options
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-g)       # Debug symbols
    add_compile_definitions(DEBUG=1)
endif()

# ============================================================================
# Source Files
# ============================================================================

set(SOURCES
    src/main.c
    src/core/shared.c
    src/core/crash.c

    # Core 0: audio capture + analysis (dsp.c and fft_q.c are also host-tested)
    src/audio/audio_task.c
    src/audio/dsp.c
    src/audio/fft_q.c

    # Core 1: rendering
    src/display/ili9341.c
    src/display/gfx.c
    src/display/font/font5x7.c
    src/display/renderer.c
    src/display/themes/theme_common.c
    src/display/themes/bars.c
    src/display/themes/rta.c
    src/display/themes/waterfall.c
    src/display/themes/terrain.c
    src/display/themes/radial.c
    src/display/themes/scope.c

    src/touch/xpt2046.c
)

# ============================================================================
# Main Executable
# ============================================================================

add_executable(${PROJECT_NAME}
    ${SOURCES}
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/include
)

target_link_libraries(${PROJECT_NAME}
    pico_stdlib
    pico_multicore
    hardware_adc
    hardware_clocks
    hardware_dma
    hardware_exception
    hardware_irq
    hardware_spi
    hardware_sync
    hardware_vreg
    hardware_watchdog
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    # Keep clk_peri on clk_sys after overclocking so SPI can reach 62.5 MHz
    PICO_CLOCK_ADJUST_PERI_CLOCK_WITH_SYS_CLOCK=1
    # Core 1 gets the whole 4 KB scratch_x bank (printf with floats is stack-hungry)
    PICO_CORE1_STACK_SIZE=0x1000
)

# ============================================================================
# Output Formats
# ============================================================================

# Generate UF2 file for drag-and-drop programming
pico_add_extra_outputs(${PROJECT_NAME})

# USB CDC serial for stats and commands (UART pins stay free)
pico_enable_stdio_usb(${PROJECT_NAME} 1)
pico_enable_stdio_uart(${PROJECT_NAME} 0)

# Print build configuration
message(STATUS "")
message(STATUS "=== Pico Spectrum Analyzer Build Configuration ===")
message(STATUS "Project: ${PROJECT_NAME}")
message(STATUS "Board: pico_w")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C Compiler: ${CMAKE_C_COMPILER}")
message(STATUS "Pico SDK: $ENV{PICO_SDK_PATH}")
message(STATUS "==================================================")
message(STATUS "")
