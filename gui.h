#ifndef GUI_H
#define GUI_H

#include <stdint.h>
#include "fonts.h"

// Pixel color array DMA'd to PIO, and pointer used by DMA channel 1
extern uint8_t  vga_data_array[153600];
extern uint8_t *address_pointer;

// Colors (3-bit RGB)
#define BLACK   0
#define RED     1
#define GREEN   2
#define YELLOW  3
#define BLUE    4
#define MAGENTA 5
#define CYAN    6
#define WHITE   7

void vga_init(void);
void drawPixel(int x, int y, char color);
void drawPictureFast(int dst_x, int dst_y, const uint8_t *pic, int pic_width, int pic_height);
void draw_text(int x, int y, const char *text, font_id_t font_id, uint8_t color);

#endif // GUI_H
