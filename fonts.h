#ifndef FONTS_H
#define FONTS_H

#include <stdint.h>
#include <stddef.h>

typedef struct
{
    uint8_t width;
    uint8_t height;
    uint8_t first_char;
    uint8_t last_char;
    const uint16_t *offsets;   // glyph start offsets in bitmap[]
    const uint8_t *widths;     // per-glyph width in pixels
    const uint8_t *bitmap;     // 1-bit packed bitmap data
} bitmap_font_t;

typedef enum
{
    FONT_60 = 0,
    FONT_40,
    FONT_30
} font_id_t;

#endif
