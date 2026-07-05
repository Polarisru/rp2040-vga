/**
 * Hunter Adams (vha3@cornell.edu)
 *
 * VGA driver using PIO assembler
 *
 * HARDWARE CONNECTIONS
 *  - GPIO 16 ---> VGA Hsync
 *  - GPIO 17 ---> VGA Vsync
 *  - GPIO 18 ---> 330 ohm resistor ---> VGA Red
 *  - GPIO 19 ---> 330 ohm resistor ---> VGA Green
 *  - GPIO 20 ---> 330 ohm resistor ---> VGA Blue
 *  - RP2040 GND ---> VGA GND
 *
 * RESOURCES USED
 *  - PIO state machines 0, 1, and 2 on PIO instance 0
 *  - DMA channels 0 and 1
 *  - 153.6 kBytes of RAM (for pixel color data)
 */
#include <stdio.h>
#include "pico/stdlib.h"
#include "gui.h"

int main()
{
    stdio_init_all();
    vga_init();

    // -------------------------------------------------------------------------
    // Application logic
    // -------------------------------------------------------------------------
    while (true)
    {
        char colors[8] = {BLACK, RED, GREEN, YELLOW, BLUE, MAGENTA, CYAN, WHITE};
        int index    = 0;
        int xcounter = 0;
        int ycounter = 0;

        for (int y = 0; y < 480; y++)
        {
            if (ycounter == 60) { ycounter = 0; index = (index + 1) % 8; }
            ycounter++;
            for (int x = 0; x < 640; x++)
            {
                if (xcounter == 80) { xcounter = 0; index = (index + 1) % 8; }
                xcounter++;
                drawPixel(x, y, colors[index]);
            }
        }
    }
}
