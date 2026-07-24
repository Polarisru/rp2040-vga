#!/usr/bin/env python3
import argparse
import pathlib
import re
import sys
from typing import List, Tuple

from PIL import Image

PALETTE_8 = [
    (0, 0, 0),         # 0 BLACK
    (255, 0, 0),       # 1 RED
    (0, 255, 0),       # 2 GREEN
    (255, 255, 0),     # 3 YELLOW
    (0, 0, 255),       # 4 BLUE
    (255, 0, 255),     # 5 MAGENTA
    (0, 255, 255),     # 6 CYAN
    (255, 255, 255),   # 7 WHITE
]

def sanitize_name(name: str) -> str:
    name = re.sub(r'[^0-9a-zA-Z_]', '_', name)
    if not name:
        name = 'image'
    if name[0].isdigit():
        name = '_' + name
    return name

def nearest_vga_color_index(rgb: Tuple[int, int, int]) -> int:
    r, g, b = rgb
    best_index = 0
    best_dist = None

    for i, (pr, pg, pb) in enumerate(PALETTE_8):
        dr = r - pr
        dg = g - pg
        db = b - pb
        dist = dr * dr + dg * dg + db * db
        if best_dist is None or dist < best_dist:
            best_dist = dist
            best_index = i

    return best_index

def convert_image_to_indices(img: Image.Image) -> Tuple[int, int, List[int]]:
    rgb = img.convert("RGB")
    width, height = rgb.size
    pixels = list(rgb.getdata())
    indices = [nearest_vga_color_index(p) for p in pixels]
    return width, height, indices

def pack_two_pixels_per_byte(width: int, height: int, indices: List[int]) -> Tuple[int, List[int]]:
    packed = []

    for y in range(height):
        row_start = y * width
        for x in range(0, width, 2):
            p0 = indices[row_start + x] & 0x07
            p1 = indices[row_start + x + 1] & 0x07 if (x + 1) < width else 0
            packed.append(p0 | (p1 << 3))

    return (width + 1) // 2, packed

def format_c_array(data: List[int], columns: int = 12) -> str:
    lines = []
    for i in range(0, len(data), columns):
        chunk = data[i:i + columns]
        lines.append("    " + ", ".join(f"0x{b:02X}" for b in chunk))
    return ",\n".join(lines)

def write_c_file(output_path: pathlib.Path,
                 var_name: str,
                 width: int,
                 height: int,
                 row_bytes: int,
                 packed: List[int],
                 source_name: str) -> None:
    guard = sanitize_name(var_name).upper() + "_H"
    array_text = format_c_array(packed)
    total_bytes = len(packed)

    content = f"""#ifndef {guard}
#define {guard}

#include <stdint.h>

// Source image: {source_name}
// Size: {width}x{height} pixels
// Format: 2 pixels per byte, low 3 bits = first pixel, bits 3..5 = second pixel

#define {var_name.upper()}_WIDTH {width}
#define {var_name.upper()}_HEIGHT {height}
#define {var_name.upper()}_ROW_BYTES {row_bytes}
#define {var_name.upper()}_DATA_SIZE {total_bytes}

static const uint8_t {var_name}[{total_bytes}] = {{
{array_text}
}};

#endif
"""
    output_path.write_text(content, encoding="utf-8")

def main() -> int:
    parser = argparse.ArgumentParser(
        description="Convert an image into packed 8-color VGA format and save as a C array file."
    )
    parser.add_argument("input", help="Input image file")
    parser.add_argument("-o", "--output", help="Output .h or .c file path")
    parser.add_argument("-n", "--name", help="C variable name")
    parser.add_argument("--width", type=int, help="Resize width in pixels")
    parser.add_argument("--height", type=int, help="Resize height in pixels")
    parser.add_argument("--no-resize", action="store_true", help="Keep original image size")
    args = parser.parse_args()

    input_path = pathlib.Path(args.input)
    if not input_path.exists():
        print(f"Input file not found: {input_path}", file=sys.stderr)
        return 1

    output_path = pathlib.Path(args.output) if args.output else input_path.with_suffix(".h")
    var_name = sanitize_name(args.name if args.name else input_path.stem)

    img = Image.open(input_path)

    if not args.no_resize and (args.width or args.height):
        new_width = args.width if args.width else img.width
        new_height = args.height if args.height else img.height
        img = img.resize((new_width, new_height), Image.Resampling.LANCZOS)

    width, height, indices = convert_image_to_indices(img)
    row_bytes, packed = pack_two_pixels_per_byte(width, height, indices)

    write_c_file(output_path, var_name, width, height, row_bytes, packed, input_path.name)

    print(f"Converted {input_path} -> {output_path}")
    print(f"Size: {width}x{height} pixels")
    print(f"Packed size: {len(packed)} bytes")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
