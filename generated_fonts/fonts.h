#ifndef FONTS_H
#define FONTS_H

#include <stdint.h>
#include <stddef.h>

typedef struct
{
    uint8_t height;
    uint8_t first_char;
    uint8_t last_char;
    const uint16_t *offsets;   // glyph start offsets in bitmap[]
    const uint8_t  *widths;    // per-glyph width in pixels
    const uint8_t  *advances;  // per-glyph horizontal advance
    const uint8_t  *bitmap;    // 1-bit packed bitmap data
} bitmap_font_t;

#define BITMAP_FONT_T_DEFINED

typedef enum
{
    FONT_64 = 0,
    FONT_48,
    FONT_32
} font_id_t;

#endif
