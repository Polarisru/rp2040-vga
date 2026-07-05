#!/usr/bin/env python3
import argparse
import pathlib
import re
import sys
from PIL import Image, ImageDraw, ImageFont

ASCII_FIRST = 32
ASCII_LAST = 126
SIZES = [30, 40, 60]


def sanitize_name(name: str) -> str:
    name = re.sub(r'[^0-9a-zA-Z_]', '_', name)
    if not name:
        name = "font"
    if name[0].isdigit():
        name = "_" + name
    return name


def bytes_per_row(width: int) -> int:
    return (width + 7) // 8


def pack_row_bits(bits):
    out = []
    cur = 0
    count = 0
    for b in bits:
        cur = (cur << 1) | (1 if b else 0)
        count += 1
        if count == 8:
            out.append(cur)
            cur = 0
            count = 0
    if count:
        cur <<= (8 - count)
        out.append(cur)
    return out


def render_glyph(font, ch, font_height, threshold):
    ascent, descent = font.getmetrics()
    advance = int(round(font.getlength(ch)))

    if advance <= 0:
        advance = max(1, font_height // 4)

    canvas_w = max(advance + 8, font_height * 2)
    canvas_h = font_height
    img = Image.new("L", (canvas_w, canvas_h), 0)
    draw = ImageDraw.Draw(img)

    bbox = font.getbbox(ch)
    if bbox is None:
        bbox = (0, 0, 0, 0)

    left, top, right, bottom = bbox
    draw_x = -left
    draw_y = 0

    # Align glyph baseline into the fixed-height canvas
    draw.text((draw_x, draw_y), ch, fill=255, font=font)

    # Crop to advance width only, fixed height
    glyph_w = max(1, advance)
    glyph = img.crop((0, 0, glyph_w, canvas_h))

    packed = []
    for y in range(canvas_h):
        row_bits = []
        for x in range(glyph_w):
            row_bits.append(1 if glyph.getpixel((x, y)) >= threshold else 0)
        packed.extend(pack_row_bits(row_bits))

    return glyph_w, advance, packed


def format_u8_array(data, cols=16):
    lines = []
    for i in range(0, len(data), cols):
        chunk = data[i:i + cols]
        lines.append("    " + ", ".join(f"0x{v:02X}" for v in chunk))
    return ",\n".join(lines) if lines else "    0x00"


def format_u16_array(data, cols=10):
    lines = []
    for i in range(0, len(data), cols):
        chunk = data[i:i + cols]
        lines.append("    " + ", ".join(str(v) for v in chunk))
    return ",\n".join(lines) if lines else "    0"


def write_header(out_path, symbol_name, pixel_height, glyph_offsets, glyph_widths, glyph_advances, bitmap):
    guard = sanitize_name(symbol_name).upper() + "_H"
    row_bytes_macro = sanitize_name(symbol_name).upper() + "_MAX_ROW_BYTES"

    max_width = max(glyph_widths) if glyph_widths else 1
    max_row_bytes = bytes_per_row(max_width)

    text = f"""#ifndef {guard}
#define {guard}

#include <stdint.h>

#ifndef BITMAP_FONT_T_DEFINED
#define BITMAP_FONT_T_DEFINED
typedef struct
{{
    uint8_t height;
    uint8_t first_char;
    uint8_t last_char;
    const uint16_t *offsets;
    const uint8_t *widths;
    const uint8_t *advances;
    const uint8_t *bitmap;
}} bitmap_font_t;
#endif

#define {sanitize_name(symbol_name).upper()}_HEIGHT {pixel_height}
#define {row_bytes_macro} {max_row_bytes}

static const uint16_t {symbol_name}_offsets[{len(glyph_offsets)}] = {{
{format_u16_array(glyph_offsets)}
}};

static const uint8_t {symbol_name}_widths[{len(glyph_widths)}] = {{
{format_u8_array(glyph_widths)}
}};

static const uint8_t {symbol_name}_advances[{len(glyph_advances)}] = {{
{format_u8_array(glyph_advances)}
}};

static const uint8_t {symbol_name}_bitmap[{len(bitmap)}] = {{
{format_u8_array(bitmap)}
}};

static const bitmap_font_t {symbol_name} = {{
    {pixel_height},
    {ASCII_FIRST},
    {ASCII_LAST},
    {symbol_name}_offsets,
    {symbol_name}_widths,
    {symbol_name}_advances,
    {symbol_name}_bitmap
}};

#endif
"""
    out_path.write_text(text, encoding="utf-8")


def generate_font(font_path, out_dir, base_name, pixel_height, threshold):
    print(f"Generating {pixel_height}px from {font_path}")
    font = ImageFont.truetype(str(font_path), pixel_height)

    offsets = []
    widths = []
    advances = []
    bitmap = []

    for code in range(ASCII_FIRST, ASCII_LAST + 1):
        ch = chr(code)
        glyph_w, advance, packed = render_glyph(font, ch, pixel_height, threshold)
        offsets.append(len(bitmap))
        widths.append(glyph_w)
        advances.append(advance)
        bitmap.extend(packed)

    symbol_name = f"{sanitize_name(base_name)}_{pixel_height}"
    out_path = out_dir / f"{symbol_name}.h"
    write_header(out_path, symbol_name, pixel_height, offsets, widths, advances, bitmap)
    print(f"Wrote {out_path}")


def main():
    parser = argparse.ArgumentParser(description="Convert a TTF font into fixed-height bitmap C headers.")
    parser.add_argument("font", help="Path to TTF font file")
    parser.add_argument("-o", "--outdir", default="generated_fonts", help="Output directory")
    parser.add_argument("-n", "--name", default="vga_font", help="Base output symbol/file name")
    parser.add_argument("--threshold", type=int, default=128, help="Monochrome threshold 0..255")
    args = parser.parse_args()

    font_path = pathlib.Path(args.font)
    if not font_path.exists():
        print(f"ERROR: font file not found: {font_path}")
        return 1

    out_dir = pathlib.Path(args.outdir)
    out_dir.mkdir(parents=True, exist_ok=True)

    for size in SIZES:
        generate_font(font_path, out_dir, args.name, size, args.threshold)

    print("Done.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
