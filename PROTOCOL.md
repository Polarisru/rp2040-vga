# UART Protocol

Serial port: **115200 8N1**, pins GPIO 0 (TX) / GPIO 1 (RX).

Each command is a single line terminated by `\n` (bare LF or CRLF both accepted).  
Maximum line length: **127 characters** (excluding the newline).  
All numeric fields are **unsigned decimal integers** — no leading zeros, no spaces, no sign.

---

## Command reference

### `0` — Frame header

```
0:<num_lines>:<font_height>
```

Announces the start of a new text frame.

| Field        | Type   | Range    | Description                               |
|--------------|--------|----------|-------------------------------------------|
| `num_lines`  | uint16 | 1..65535 | Number of text lines that will follow     |
| `font`       | uint8  | 0..2     | Font size (see [Font table](#font-table)) |

Must be sent before any numbered text lines. Resets the receiver's line counter.

**Example**
```
0:4:2
```

---

### `<N>` — Numbered text line

```
<N>:<text>:<color>
```

Draws a line of text at a logical row index. Position on screen is determined by the application using `font_height` from the preceding header.

| Field   | Type   | Range    | Description                                                   |
|---------|--------|----------|---------------------------------------------------------------|
| `N`     | uint16 | 1..65535 | Line index (must be ≤ `num_lines` from the current header)    |
| `color` | uint8  | 0..7     | VGA color index (see [Color table](#color-table))             |
| `text`  | string | —        | Arbitrary text; may contain colons                            |

The parser splits on the **last** colon to locate `color`, so `text` may contain colons freely.

**Example**
```
1:7:Hello, world!
2:3:Status: OK
```

---

### `F` — Fill screen

```
F:<color>
```

Fills the entire 640×480 framebuffer with a solid color using a single `memset`.

| Field   | Type  | Range | Description                                   |
|---------|-------|-------|-----------------------------------------------|
| `color` | uint8 | 0..7  | VGA color index (see [Color table](#color-table)) |

**Example**
```
F:0
```
*(fills screen black)*

---

### `R` — Draw rectangle

```
R:<x1>:<y1>:<x2>:<y2>:<color>
```

Draws a filled rectangle. Corner order does not matter — coordinates are sorted internally.  
Out-of-bounds coordinates are clamped to the screen edges (0..639 / 0..479).

| Field   | Type   | Range  | Description                                   |
|---------|--------|--------|-----------------------------------------------|
| `x1`    | uint16 | 0..639 | X coordinate of the first corner             |
| `y1`    | uint16 | 0..479 | Y coordinate of the first corner             |
| `x2`    | uint16 | 0..639 | X coordinate of the opposite corner          |
| `y2`    | uint16 | 0..479 | Y coordinate of the opposite corner          |
| `color` | uint8  | 0..7   | VGA color index (see [Color table](#color-table)) |

**Example**
```
R:10:20:200:100:3
```
*(draws a yellow filled rectangle from (10,20) to (200,100))*

---

### `T` — Draw text

```
T:<x>:<y>:<color>:<font>:<text>
```

Renders a UTF-8 text string at a pixel-precise position using a bitmap font.  
The text field is everything after the fifth colon, so it may contain colons.

| Field   | Type   | Range  | Description                                   |
|---------|--------|--------|-----------------------------------------------|
| `x`     | uint16 | 0..639 | Pixel X of the top-left of the first glyph   |
| `y`     | uint16 | 0..479 | Pixel Y of the top-left of the first glyph   |
| `color` | uint8  | 0..7   | VGA color index (see [Color table](#color-table)) |
| `font`  | uint8  | 0..2   | Font size (see [Font table](#font-table))     |
| `text`  | string | —      | Text to render; may contain colons            |

**Example**
```
T:50:120:7:1:Temperature: 23.5 C
```
*(renders white text at (50,120) using the medium font)*

---

## Color table

| Index | Name    |
|-------|---------|
| 0     | Black   |
| 1     | Red     |
| 2     | Green   |
| 3     | Yellow  |
| 4     | Blue    |
| 5     | Magenta |
| 6     | Cyan    |
| 7     | White   |

Colors are 3-bit RGB (1 bit per channel). The VGA DAC uses 330 Ω resistors on GPIO 18–20.

---

## Font table

| Index | Glyph height |
|-------|--------------|
| 0     | 60 px        |
| 1     | 40 px        |
| 2     | 30 px        |

---

## Error handling

If a line cannot be parsed (missing field, non-numeric value, out-of-range value, or buffer overflow), the receiver calls the registered error callback with the offending raw token and a short reason string. The line is discarded and reception continues normally from the next newline.
