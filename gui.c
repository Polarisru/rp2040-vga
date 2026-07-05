#include <stdint.h>
#include <string.h>
#include "fonts.h"
#include "gui.h"

// Length of the pixel array: 640*480 pixels / 2 pixels per byte
#define TXCOUNT 153600

uint8_t  vga_data_array[TXCOUNT];
uint8_t *address_pointer = &vga_data_array[0];

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
    if (x < 0 || x >= 640 || y < 0 || y >= 480) return;

    int pixel = y * 640 + x;
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

void drawPixel(int x, int y, char color)
{
    if (x > 639) x = 639;
    if (x < 0)   x = 0;
    if (y < 0)   y = 0;
    if (y > 479) y = 479;

    int pixel = (640 * y) + x;

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
    if (pic == NULL) return;
    if (pic_width <= 0 || pic_height <= 0) return;
    if (dst_x < 0 || dst_y < 0) return;
    if ((dst_x + pic_width)  > 640) return;
    if ((dst_y + pic_height) > 480) return;
    if ((dst_x    & 1) != 0) return;
    if ((pic_width & 1) != 0) return;

    const int src_bytes_per_row = pic_width >> 1;
    const int dst_bytes_per_row = 640 >> 1;

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
