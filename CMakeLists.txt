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
    # Real spectrum analyzer (active)
    src/spectrum_analyzer.c
    
    # Display driver and themes
    src/display/ili9341.c
    src/display/theme_manager.c
    src/display/themes/bars.c
    src/display/themes/waterfall.c
    src/display/themes/radial.c
    src/display/themes/mirror.c
    
    # Touch controller
    src/touch/xpt2046.c
    
    # Audio processing
    src/audio/adc_sampler.c
    src/audio/fft_processor.c
    
    # Optional test/development modules (comment out for release)
    # src/spectrum_viz_test.c
    # src/utils/mock_audio.c
    # src/display_test.c
    # src/main.c
    # src/main_simple_test.c
)

# ============================================================================
# PIO Programs
# ============================================================================

# Generate PIO headers from .pio files
# The pico_generate_pio_header function compiles .pio assembly into C headers
set(PIO_SOURCES
    # pio/adc_sampler.pio  # To be added
)

# ============================================================================
# Main Executable
# ============================================================================

add_executable(${PROJECT_NAME}
    ${SOURCES}
)

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/include
    ${CMAKE_CURRENT_LIST_DIR}/include/audio
    ${CMAKE_CURRENT_LIST_DIR}/include/display
    ${CMAKE_CURRENT_LIST_DIR}/include/touch
    ${CMAKE_CURRENT_LIST_DIR}/include/utils
)

# Link Pico SDK libraries
target_link_libraries(${PROJECT_NAME}
    pico_stdlib              # Standard library (stdio, time, etc.)
    hardware_gpio            # GPIO control
    pico_multicore           # Dual-core support
    hardware_adc             # ADC for audio input
    hardware_spi             # SPI for display and touch
    hardware_dma             # DMA for efficient transfers
    hardware_pio             # PIO for custom peripherals
    hardware_pwm             # PWM (if needed for backlight)
    pico_binary_info         # Binary metadata for picotool
)

# Generate PIO headers
# foreach(PIO_FILE ${PIO_SOURCES})
#     pico_generate_pio_header(${PROJECT_NAME} ${CMAKE_CURRENT_LIST_DIR}/${PIO_FILE})
# endforeach()

# ============================================================================
# Output Formats
# ============================================================================

# Generate UF2 file for drag-and-drop programming
pico_add_extra_outputs(${PROJECT_NAME})

# Enable USB serial output (comment out to use UART instead)
pico_enable_stdio_usb(${PROJECT_NAME} 1)
pico_enable_stdio_uart(${PROJECT_NAME} 1)

# ============================================================================
# CMSIS-DSP Library (for FFT)
# ============================================================================

# TODO: Add CMSIS-DSP as a submodule and link it here
# For now, this is a placeholder
# add_subdirectory(lib/CMSIS-DSP)
# target_link_libraries(${PROJECT_NAME} CMSISDSP)

# ============================================================================
# Build Information
# ============================================================================

# Add compile definitions
target_compile_definitions(${PROJECT_NAME} PRIVATE
    # PICO_BOARD is set via CMake command line
    # Add custom defines here
)

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

# ============================================================================
# Testing (Optional)
# ============================================================================

option(BUILD_TESTS "Build unit tests" OFF)

if(BUILD_TESTS)
    enable_testing()
    # add_subdirectory(tests)
    message(STATUS "Unit tests enabled")
endif()

