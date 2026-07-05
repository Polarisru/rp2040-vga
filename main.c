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
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hsync.pio.h"
#include "vsync.pio.h"
#include "rgb.pio.h"
#include "gui.h"

// VGA timing constants
#define H_ACTIVE   655    // (active + frontporch - 1) - one cycle delay for mov
#define V_ACTIVE   479    // (active - 1)
#define RGB_ACTIVE 319    // (horizontal active)/2 - 1

// GPIO pin assignments
#define HSYNC     16
#define VSYNC     17
#define RED_PIN   18

int main()
{
    stdio_init_all();

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

    // -------------------------------------------------------------------------
    // DMA channels: 0 sends pixel data, 1 reconfigures and restarts 0
    // -------------------------------------------------------------------------
    int rgb_chan_0 = 0;
    int rgb_chan_1 = 1;

    dma_channel_config c0 = dma_channel_get_default_config(rgb_chan_0);
    channel_config_set_transfer_data_size(&c0, DMA_SIZE_8);
    channel_config_set_read_increment(&c0, true);
    channel_config_set_write_increment(&c0, false);
    channel_config_set_dreq(&c0, DREQ_PIO0_TX2);
    channel_config_set_chain_to(&c0, rgb_chan_1);

    dma_channel_configure(
        rgb_chan_0,
        &c0,
        &pio->txf[rgb_sm],   // write: RGB PIO TX FIFO
        &vga_data_array,     // read:  pixel color array
        153600,
        false
    );

    dma_channel_config c1 = dma_channel_get_default_config(rgb_chan_1);
    channel_config_set_transfer_data_size(&c1, DMA_SIZE_32);
    channel_config_set_read_increment(&c1, false);
    channel_config_set_write_increment(&c1, false);
    channel_config_set_chain_to(&c1, rgb_chan_0);

    dma_channel_configure(
        rgb_chan_1,
        &c1,
        &dma_hw->ch[rgb_chan_0].read_addr,  // write: channel 0 read-address register
        &address_pointer,                   // read:  pointer to pixel array address
        1,
        false
    );

    // Push initial counts to PIO state machines
    pio_sm_put_blocking(pio, hsync_sm, H_ACTIVE);
    pio_sm_put_blocking(pio, vsync_sm, V_ACTIVE);
    pio_sm_put_blocking(pio, rgb_sm,   RGB_ACTIVE);

    // Start all three state machines in sync, then kick off DMA
    pio_enable_sm_mask_in_sync(pio, (1u << hsync_sm) | (1u << vsync_sm) | (1u << rgb_sm));
    dma_start_channel_mask(1u << rgb_chan_0);

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
