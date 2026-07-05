#ifndef GUI_H
#define GUI_H

#include <stdint.h>
#include "fonts.h"

#define GUI_WIDTH     640
#define GUI_HEIGHT    480
// Length of the pixel array: 640*480 pixels / 2 pixels per byte
#define GUI_SIZE      (GUI_WIDTH * GUI_HEIGHT / 2)

// Pixel color array DMA'd to PIO, and pointer used by DMA channel 1
extern uint8_t  vga_data_array[GUI_SIZE];
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
void fill_screen(uint8_t color);
void draw_rect(int x1, int y1, int x2, int y2, uint8_t color);
void drawPixel(int x, int y, char color);
void drawPictureFast(int dst_x, int dst_y, const uint8_t *pic, int pic_width, int pic_height);
void draw_text(int x, int y, const char *text, font_id_t font_id, uint8_t color);

#endif // GUI_H
