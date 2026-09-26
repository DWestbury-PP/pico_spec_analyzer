/**
 * @file ili9341.c
 * @brief ILI9341 driver: register setup plus IRQ-driven indexed-frame streaming.
 *
 * Streaming works in 8-row strips. While DMA sends strip s, the IRQ that
 * started it palette-expands strip s+1 into the other buffer. The CPU cost is
 * about 25 us per 0.65 ms strip, and the SPI never idles for longer than IRQ entry.
 */

#include "display/ili9341.h"
#include "config.h"
#include "platform.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "pico/time.h"
#include <string.h>

#define NSTRIPS        (DISPLAY_HEIGHT / ILI9341_STRIP_H)
#define STRIP_PIXELS   (DISPLAY_WIDTH * ILI9341_STRIP_H)
#define READ_HZ        (6 * 1000 * 1000)   // RAMRD read cycle is 150 ns min

static uint16_t _width = ILI9341_TFTWIDTH;
static uint16_t _height = ILI9341_TFTHEIGHT;
static uint32_t _stream_hz;

static uint16_t _strip_buf[2][STRIP_PIXELS];
static const uint8_t *_fb;
static const uint16_t *_pal;
static volatile int _strip;
static volatile bool _busy;
static volatile uint64_t _done_us;
static int _dma_ch = -1;

// ============================================================================
// Low-level SPI
// ============================================================================

static inline void cs(bool level) { gpio_put(DISPLAY_PIN_CS, level); }
static inline void dc(bool level) { gpio_put(DISPLAY_PIN_DC, level); }

static void write_command(uint8_t cmd) {
    dc(0); cs(0);
    spi_write_blocking(DISPLAY_SPI_PORT, &cmd, 1);
    cs(1);
}

static void write_data_buf(const uint8_t *buf, size_t len) {
    dc(1); cs(0);
    spi_write_blocking(DISPLAY_SPI_PORT, buf, len);
    cs(1);
}

static void write_command_data(uint8_t cmd, const uint8_t *data, size_t len) {
    write_command(cmd);
    if (len) write_data_buf(data, len);
}

static void set_addr_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    uint8_t ca[4] = { x0 >> 8, x0 & 0xFF, x1 >> 8, x1 & 0xFF };
    uint8_t pa[4] = { y0 >> 8, y0 & 0xFF, y1 >> 8, y1 & 0xFF };
    write_command_data(ILI9341_CASET, ca, 4);
    write_command_data(ILI9341_PASET, pa, 4);
    write_command(ILI9341_RAMWR);
}

/**
 * SPI mode 3: the PL022 inserts an idle gap between words when CPHA = 0, but
 * streams back-to-back when CPHA = 1. The ILI9341 samples on the rising edge
 * in both mode 0 and mode 3.
 */
static void spi_16bit(bool on) {
    spi_set_format(DISPLAY_SPI_PORT, on ? 16 : 8, SPI_CPOL_1, SPI_CPHA_1, SPI_MSB_FIRST);
}

/** Discard RX data left behind by TX-only transfers and clear the overrun flag. */
static void spi_drain_rx(void) {
    while (spi_is_readable(DISPLAY_SPI_PORT)) (void)spi_get_hw(DISPLAY_SPI_PORT)->dr;
    spi_get_hw(DISPLAY_SPI_PORT)->icr = SPI_SSPICR_RORIC_BITS;
}

// ============================================================================
// Initialization
// ============================================================================

bool ili9341_init(void) {
    spi_init(DISPLAY_SPI_PORT, DISPLAY_SPI_INIT_HZ);
    spi_16bit(false);

    gpio_set_function(DISPLAY_PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(DISPLAY_PIN_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(DISPLAY_PIN_MISO, GPIO_FUNC_SPI);
    // Fast edges for 62.5 MHz over jumper wires
    gpio_set_drive_strength(DISPLAY_PIN_SCK, GPIO_DRIVE_STRENGTH_12MA);
    gpio_set_drive_strength(DISPLAY_PIN_MOSI, GPIO_DRIVE_STRENGTH_12MA);
    gpio_set_slew_rate(DISPLAY_PIN_SCK, GPIO_SLEW_RATE_FAST);
    gpio_set_slew_rate(DISPLAY_PIN_MOSI, GPIO_SLEW_RATE_FAST);

    gpio_init(DISPLAY_PIN_CS);  gpio_set_dir(DISPLAY_PIN_CS, GPIO_OUT);  cs(1);
    gpio_init(DISPLAY_PIN_DC);  gpio_set_dir(DISPLAY_PIN_DC, GPIO_OUT);  dc(1);
    gpio_init(DISPLAY_PIN_RST); gpio_set_dir(DISPLAY_PIN_RST, GPIO_OUT);
    gpio_init(DISPLAY_PIN_BL);  gpio_set_dir(DISPLAY_PIN_BL, GPIO_OUT);  gpio_put(DISPLAY_PIN_BL, 1);

    gpio_put(DISPLAY_PIN_RST, 1); sleep_ms(5);
    gpio_put(DISPLAY_PIN_RST, 0); sleep_ms(20);
    gpio_put(DISPLAY_PIN_RST, 1); sleep_ms(150);

    write_command(ILI9341_SWRESET);
    sleep_ms(150);

    write_command_data(0xCB, (const uint8_t[]){0x39, 0x2C, 0x00, 0x34, 0x02}, 5);  // power control A
    write_command_data(0xCF, (const uint8_t[]){0x00, 0xC1, 0x30}, 3);              // power control B
    write_command_data(0xE8, (const uint8_t[]){0x85, 0x00, 0x78}, 3);              // driver timing A
    write_command_data(0xEA, (const uint8_t[]){0x00, 0x00}, 2);                    // driver timing B
    write_command_data(0xED, (const uint8_t[]){0x64, 0x03, 0x12, 0x81}, 4);        // power-on sequence
    write_command_data(0xF7, (const uint8_t[]){0x20}, 1);                          // pump ratio
    write_command_data(ILI9341_PWCTR1, (const uint8_t[]){0x23}, 1);
    write_command_data(ILI9341_PWCTR2, (const uint8_t[]){0x10}, 1);
    write_command_data(ILI9341_VMCTR1, (const uint8_t[]){0x3e, 0x28}, 2);
    write_command_data(ILI9341_VMCTR2, (const uint8_t[]){0x86}, 1);
    write_command_data(ILI9341_MADCTL, (const uint8_t[]){0x48}, 1);
    write_command_data(ILI9341_PIXFMT, (const uint8_t[]){0x55}, 1);                // 16-bit
    write_command_data(ILI9341_FRMCTR1, (const uint8_t[]){0x00, 0x18}, 2);         // 79 Hz panel refresh
    write_command_data(ILI9341_DFUNCTR, (const uint8_t[]){0x08, 0x82, 0x27}, 3);
    write_command_data(0xF2, (const uint8_t[]){0x00}, 1);                          // 3-gamma off
    write_command_data(ILI9341_GAMMASET, (const uint8_t[]){0x01}, 1);
    write_command_data(ILI9341_GMCTRP1, (const uint8_t[]){
        0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08, 0x4E, 0xF1, 0x37, 0x07, 0x10, 0x03, 0x0E, 0x09, 0x00}, 15);
    write_command_data(ILI9341_GMCTRN1, (const uint8_t[]){
        0x00, 0x0E, 0x14, 0x03, 0x11, 0x07, 0x31, 0xC1, 0x48, 0x08, 0x0F, 0x0C, 0x31, 0x36, 0x0F}, 15);

    write_command(ILI9341_SLPOUT);
    sleep_ms(120);
    write_command(ILI9341_DISPON);
    sleep_ms(20);

    _stream_hz = spi_set_baudrate(DISPLAY_SPI_PORT, DISPLAY_SPI_STREAM_HZ);
    return true;
}

void ili9341_set_rotation(uint8_t rotation) {
    static const uint8_t madctl[4] = { 0x48, 0x28, 0x88, 0xE8 };
    rotation &= 3;
    bool landscape = rotation & 1;
    _width = landscape ? ILI9341_TFTHEIGHT : ILI9341_TFTWIDTH;
    _height = landscape ? ILI9341_TFTWIDTH : ILI9341_TFTHEIGHT;
    write_command_data(ILI9341_MADCTL, &madctl[rotation], 1);
}

void ili9341_fill_screen(uint16_t color) {
    set_addr_window(0, 0, _width - 1, _height - 1);
    dc(1); cs(0);
    spi_16bit(true);
    for (uint32_t i = 0; i < (uint32_t)_width * _height; i++) {
        spi_write16_blocking(DISPLAY_SPI_PORT, &color, 1);
    }
    while (spi_is_busy(DISPLAY_SPI_PORT)) tight_loop_contents();
    spi_16bit(false);
    spi_drain_rx();
    cs(1);
}

uint16_t ili9341_width(void)  { return _width; }
uint16_t ili9341_height(void) { return _height; }
uint32_t ili9341_stream_hz(void) { return _stream_hz; }

// ============================================================================
// Readback
// ============================================================================

uint8_t ili9341_read_reg(uint8_t reg, uint8_t index) {
    spi_set_baudrate(DISPLAY_SPI_PORT, READ_HZ);
    uint8_t idx = 0x10 + index;
    write_command_data(0xD9, &idx, 1);
    uint8_t v = 0;
    dc(0); cs(0);
    spi_write_blocking(DISPLAY_SPI_PORT, &reg, 1);
    dc(1);
    spi_read_blocking(DISPLAY_SPI_PORT, 0x00, &v, 1);
    cs(1);
    spi_set_baudrate(DISPLAY_SPI_PORT, _stream_hz);
    return v;
}

int ili9341_selftest(void) {
    enum { W = 32, H = 8, N = W * H };
    static uint16_t pat[N];
    static uint8_t rd[1 + 3 * N];
    uint32_t seed = 0x1234567u;
    for (int i = 0; i < N; i++) {
        seed = seed * 1664525u + 1013904223u;
        uint16_t v5 = (seed >> 11) & 0x1F, g6 = (seed >> 20) & 0x3F;
        pat[i] = (uint16_t)((v5 << 11) | (g6 << 5) | v5);   // R == B: immune to BGR order
    }

    // Write at streaming speed
    set_addr_window(0, 0, W - 1, H - 1);
    dc(1); cs(0);
    spi_16bit(true);
    spi_write16_blocking(DISPLAY_SPI_PORT, pat, N);
    while (spi_is_busy(DISPLAY_SPI_PORT)) tight_loop_contents();
    spi_16bit(false);
    spi_drain_rx();
    cs(1);

    // Read back slowly
    spi_set_baudrate(DISPLAY_SPI_PORT, READ_HZ);
    uint8_t ca[4] = { 0, 0, 0, W - 1 }, pa[4] = { 0, 0, 0, H - 1 };
    write_command_data(ILI9341_CASET, ca, 4);
    write_command_data(ILI9341_PASET, pa, 4);
    uint8_t cmd = ILI9341_RAMRD;
    dc(0); cs(0);
    spi_write_blocking(DISPLAY_SPI_PORT, &cmd, 1);
    dc(1);
    spi_read_blocking(DISPLAY_SPI_PORT, 0x00, rd, sizeof(rd));
    cs(1);
    spi_set_baudrate(DISPLAY_SPI_PORT, _stream_hz);

    bool all_same = true;
    for (size_t i = 2; i < sizeof(rd); i++) if (rd[i] != rd[1]) { all_same = false; break; }
    if (all_same) return -1;

    int bad = 0;
    for (int i = 0; i < N; i++) {
        const uint8_t *p = &rd[1 + 3 * i];
        uint16_t v5 = pat[i] >> 11, g6 = (pat[i] >> 5) & 0x3F;
        if ((p[0] >> 3) != v5 || (p[1] >> 2) != g6 || (p[2] >> 3) != v5) bad++;
    }
    return bad;
}

// ============================================================================
// Streaming
// ============================================================================

static void RAMFUNC(convert_strip)(int s, uint16_t *dst) {
    const uint32_t *src = (const uint32_t *)(_fb + s * STRIP_PIXELS);
    const uint16_t *pal = _pal;
    for (int n = STRIP_PIXELS / 4; n; n--) {
        uint32_t w = *src++;
        dst[0] = pal[w & 0xFF];
        dst[1] = pal[(w >> 8) & 0xFF];
        dst[2] = pal[(w >> 16) & 0xFF];
        dst[3] = pal[w >> 24];
        dst += 4;
    }
}

static void RAMFUNC(stream_irq)(void) {
    dma_hw->ints1 = 1u << _dma_ch;
    int s = _strip + 1;
    if (s < NSTRIPS) {
        _strip = s;
        dma_channel_transfer_from_buffer_now(_dma_ch, _strip_buf[s & 1], STRIP_PIXELS);
        if (s + 1 < NSTRIPS) convert_strip(s + 1, _strip_buf[(s + 1) & 1]);
    } else {
        _done_us = time_us_64();
        _busy = false;
    }
}

void ili9341_stream_init(void) {
    _dma_ch = dma_claim_unused_channel(true);
    dma_channel_config c = dma_channel_get_default_config(_dma_ch);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_16);
    channel_config_set_read_increment(&c, true);
    channel_config_set_write_increment(&c, false);
    channel_config_set_dreq(&c, spi_get_dreq(DISPLAY_SPI_PORT, true));
    dma_channel_configure(_dma_ch, &c, &spi_get_hw(DISPLAY_SPI_PORT)->dr, NULL, STRIP_PIXELS, false);

    dma_channel_set_irq1_enabled(_dma_ch, true);
    irq_set_exclusive_handler(DMA_IRQ_1, stream_irq);
    irq_set_enabled(DMA_IRQ_1, true);
}

void ili9341_stream_begin(const uint8_t *fb, const uint16_t *palette) {
    _fb = fb;
    _pal = palette;
    set_addr_window(0, 0, DISPLAY_WIDTH - 1, DISPLAY_HEIGHT - 1);
    dc(1); cs(0);
    spi_16bit(true);

    _busy = true;
    _strip = 0;
    convert_strip(0, _strip_buf[0]);
    dma_channel_transfer_from_buffer_now(_dma_ch, _strip_buf[0], STRIP_PIXELS);
    convert_strip(1, _strip_buf[1]);
}

bool ili9341_stream_busy(void) {
    return _busy;
}

uint64_t ili9341_stream_done_us(void) {
    return _done_us;
}

void ili9341_stream_wait(void) {
    while (_busy) tight_loop_contents();
    while (spi_is_busy(DISPLAY_SPI_PORT)) tight_loop_contents();
    cs(1);
    spi_16bit(false);
    spi_drain_rx();
}
