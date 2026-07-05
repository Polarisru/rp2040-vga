#include <stdint.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hsync.pio.h"
#include "vsync.pio.h"
#include "rgb.pio.h"
#include "fonts.h"
#include "gui.h"

// VGA timing constants
#define H_ACTIVE   655    // (active + frontporch - 1) - one cycle delay for mov
#define V_ACTIVE   479    // (active - 1)
#define RGB_ACTIVE 319    // (horizontal active)/2 - 1

// GPIO pin assignments
#define HSYNC    16
#define VSYNC    17
#define RED_PIN  18

uint8_t  vga_data_array[GUI_SIZE];
uint8_t *address_pointer = &vga_data_array[0];

// ---------------------------------------------------------------------------
// VGA initialisation
// ---------------------------------------------------------------------------

void vga_init(void)
{
    PIO pio = pio0;

    uint hsync_offset = pio_add_program(pio, &hsync_program);
    uint vsync_offset = pio_add_program(pio, &vsync_program);
    uint rgb_offset   = pio_add_program(pio, &rgb_program);

    uint hsync_sm = 0;
    uint vsync_sm = 1;
    uint rgb_sm   = 2;

    hsync_program_init(pio, hsync_sm, hsync_offset, HSYNC);
    vsync_program_init(pio, vsync_sm, vsync_offset, VSYNC);
    rgb_program_init  (pio, rgb_sm,   rgb_offset,   RED_PIN);

    // Channel 0: sends pixel data to the RGB PIO TX FIFO
    int rgb_chan_0 = 0;
    int rgb_chan_1 = 1;

    dma_channel_config c0 = dma_channel_get_default_config(rgb_chan_0);
    channel_config_set_transfer_data_size(&c0, DMA_SIZE_8);
    channel_config_set_read_increment(&c0, true);
    channel_config_set_write_increment(&c0, false);
    channel_config_set_dreq(&c0, DREQ_PIO0_TX2);
    channel_config_set_chain_to(&c0, rgb_chan_1);

    dma_channel_configure(
        rgb_chan_0, &c0,
        &pio->txf[rgb_sm],  // write: RGB PIO TX FIFO
        &vga_data_array,    // read:  pixel color array
        GUI_SIZE,
        false
    );

    // Channel 1: resets channel 0's read address and restarts it
    dma_channel_config c1 = dma_channel_get_default_config(rgb_chan_1);
    channel_config_set_transfer_data_size(&c1, DMA_SIZE_32);
    channel_config_set_read_increment(&c1, false);
    channel_config_set_write_increment(&c1, false);
    channel_config_set_chain_to(&c1, rgb_chan_0);

    dma_channel_configure(
        rgb_chan_1, &c1,
        &dma_hw->ch[rgb_chan_0].read_addr,  // write: channel 0 read-address register
        &address_pointer,                   // read:  pointer to pixel array address
        1,
        false
    );

    // Push initial counts into PIO state machines
    pio_sm_put_blocking(pio, hsync_sm, H_ACTIVE);
    pio_sm_put_blocking(pio, vsync_sm, V_ACTIVE);
    pio_sm_put_blocking(pio, rgb_sm,   RGB_ACTIVE);

    // Start all state machines in sync, then kick off DMA
    pio_enable_sm_mask_in_sync(pio, (1u << hsync_sm) | (1u << vsync_sm) | (1u << rgb_sm));
    dma_start_channel_mask(1u << rgb_chan_0);
}

// ---------------------------------------------------------------------------
// Font table
// ---------------------------------------------------------------------------

extern const bitmap_font_t g_font_60;
extern const bitmap_font_t g_font_40;
extern const bitmap_font_t g_font_30;

static const bitmap_font_t *get_font(font_id_t id)
{
    switch (id)
    {
        case FONT_60: return &g_font_60;
        case FONT_40: return &g_font_40;
        case FONT_30: return &g_font_30;
        default:      return &g_font_30;
    }
}

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

static void drawPixelMasked(int x, int y, uint8_t color)
{
    if (x < 0 || x >= GUI_WIDTH || y < 0 || y >= GUI_HEIGHT) 
      return;

    int pixel = y * GUI_WIDTH + x;
    uint8_t *p = &vga_data_array[pixel >> 1];
    color &= 0x07;

    if (pixel & 1)
        *p = (uint8_t)((*p & 0x07) | (color << 3));
    else
        *p = (uint8_t)((*p & 0x38) | color);
}

static void draw_char(int x, int y, char c, const bitmap_font_t *font, uint8_t color)
{
    if (font == NULL) return;
    if ((uint8_t)c < font->first_char || (uint8_t)c > font->last_char) return;

    uint16_t glyph_index = (uint8_t)c - font->first_char;
    uint16_t bit_offset  = font->offsets[glyph_index];
    uint8_t  glyph_width  = font->widths[glyph_index];
    uint8_t  glyph_height = font->height;

    for (uint8_t gy = 0; gy < glyph_height; gy++)
    {
        for (uint8_t gx = 0; gx < glyph_width; gx++)
        {
            uint32_t bit_index = bit_offset + (uint32_t)gy * glyph_width + gx;
            uint8_t  byte      = font->bitmap[bit_index >> 3];
            uint8_t  bit       = (byte >> (7 - (bit_index & 7))) & 0x01;

            if (bit)
                drawPixelMasked(x + gx, y + gy, color);
        }
    }
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void fill_screen(uint8_t color)
{
    memset(vga_data_array, (color & 0x07) | ((color & 0x07) << 3), GUI_SIZE);
}

void draw_rect(int x1, int y1, int x2, int y2, uint8_t color)
{
    if (x1 > x2) { int t = x1; x1 = x2; x2 = t; }
    if (y1 > y2) { int t = y1; y1 = y2; y2 = t; }
    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 >= GUI_WIDTH)  x2 = GUI_WIDTH  - 1;
    if (y2 >= GUI_HEIGHT) y2 = GUI_HEIGHT - 1;

    for (int y = y1; y <= y2; y++)
        for (int x = x1; x <= x2; x++)
            drawPixelMasked(x, y, color);
}

void drawPixel(int x, int y, char color)
{
    if (x >= GUI_WIDTH) 
      x = GUI_WIDTH - 1;
    if (x < 0)   
      x = 0;
    if (y < 0)   
      y = 0;
    if (y >= GUI_HEIGHT) 
      y = GUI_HEIGHT - 1;

    int pixel = (GUI_WIDTH * y) + x;

    if (pixel & 1)
        vga_data_array[pixel >> 1] |= (color << 3);
    else
        vga_data_array[pixel >> 1] |= (color);
}

void drawPictureFast(int dst_x, int dst_y,
                     const uint8_t *pic,
                     int pic_width,
                     int pic_height)
{
    if (pic == NULL) 
      return;
    if (pic_width <= 0 || pic_height <= 0) 
      return;
    if (dst_x < 0 || dst_y < 0) 
      return;
    if ((dst_x + pic_width)  > GUI_WIDTH) 
      return;
    if ((dst_y + pic_height) > GUI_HEIGHT) 
      return;
    if ((dst_x & 1) != 0) 
      return;
    if ((pic_width & 1) != 0) 
      return;

    const int src_bytes_per_row = pic_width >> 1;
    const int dst_bytes_per_row = GUI_WIDTH >> 1;

    for (int y = 0; y < pic_height; y++)
    {
        uint8_t       *dst = &vga_data_array[(dst_y + y) * dst_bytes_per_row + (dst_x >> 1)];
        const uint8_t *src = &pic[y * src_bytes_per_row];
        memcpy(dst, src, (size_t)src_bytes_per_row);
    }
}

void draw_text(int x, int y, const char *text, font_id_t font_id, uint8_t color)
{
    const bitmap_font_t *font = get_font(font_id);
    int cursor_x = x;

    if (text == NULL || font == NULL) return;

    while (*text != '\0')
    {
        char c = *text++;

        if (c == '\n')
        {
            cursor_x = x;
            y += font->height;
            continue;
        }

        if (c == ' ')
        {
            cursor_x += font->height / 3;
            continue;
        }

        if ((uint8_t)c >= font->first_char && (uint8_t)c <= font->last_char)
        {
            draw_char(cursor_x, y, c, font, color);
            cursor_x += font->widths[(uint8_t)c - font->first_char] + 1;
        }
    }
}
