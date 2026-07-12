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
#include "hardware/clocks.h"
#include "gui.h"
#include "volz.h"
#include "uart_rx.h"

#define APP_UART              uart0
#define APP_UART_TX_PIN       0
#define APP_UART_RX_PIN       1
#define APP_UART_BAUD         115200
#define UART_RX_BUFFER_SIZE   128

typedef struct
{
    uint8_t max_lines;
    uint8_t font;
} logic_context_t;

static void logic_on_uart_message(const uart_rx_message_t *message, void *user_ctx)
{
    logic_context_t *logic = (logic_context_t *)user_ctx;

    switch (message->type)
    {
        case UART_RX_LINE_TYPE_HEADER:
            logic->max_lines  = message->data.header.number_of_lines;
            logic->font = message->data.header.font;
            break;

        case UART_RX_LINE_TYPE_TEXT:
            if (message->line_index <= logic->max_lines)
            {
                draw_text(10,
                      10 + (message->line_index - 1) * get_font_height((font_id_t)logic->font),
                      message->data.text.text,
                      (font_id_t)logic->font,
                      message->data.draw_text.color);
            }
            break;

        case UART_RX_CMD_FILL:
            fill_screen(message->data.fill.color);
            break;

        case UART_RX_CMD_RECT:
            draw_rect(message->data.rect.x1,
                      message->data.rect.y1,
                      message->data.rect.x2,
                      message->data.rect.y2,
                      message->data.rect.color);
            break;
            
        case UART_RX_CMD_BOX:
            draw_box(message->data.box.x1,
                       message->data.box.y1,
                       message->data.box.x2,
                       message->data.box.y2,
                       message->data.box.width,
                       message->data.box.color);
            break;            

        case UART_RX_CMD_TEXT:
            draw_text(message->data.draw_text.x,
                      message->data.draw_text.y,
                      message->data.draw_text.text,
                      (font_id_t)message->data.draw_text.font,
                      message->data.draw_text.color);
            break;

        default:
            break;
    }
}

static void logic_on_uart_error(const char *raw_line, const char *reason, void *user_ctx)
{
    (void)raw_line;
    (void)reason;
    (void)user_ctx;
}

int main()
{
    static char rx_buffer[UART_RX_BUFFER_SIZE];
    static uart_rx_t uart_rx;
    static logic_context_t logic = {0};
    
    set_sys_clock_khz(126000, true);

    stdio_init_all();
    vga_init();
    uart_init(APP_UART, APP_UART_BAUD);
    gpio_set_function(APP_UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(APP_UART_RX_PIN, GPIO_FUNC_UART);

    uart_rx_init(&uart_rx,
                 APP_UART,
                 rx_buffer,
                 sizeof(rx_buffer),
                 logic_on_uart_message,
                 logic_on_uart_error,
                 &logic);
                 
    fill_screen(WHITE);
    drawPictureFast((GUI_WIDTH - 436) / 2, (GUI_HEIGHT - 264) / 2, volz, 436, 264);

    /*char colors[8] = {BLACK, RED, GREEN, YELLOW, BLUE, MAGENTA, CYAN, WHITE};
    int index    = 0;
    int xcounter = 0;
    int ycounter = 0;

    for (int y = 0; y < GUI_HEIGHT; y++)
    {
        if (ycounter == GUI_HEIGHT / 8) 
        { 
          ycounter = 0; 
          index = (index + 1) % 8; 
        }
        ycounter++;
        for (int x = 0; x < GUI_WIDTH; x++)
        {
            if (xcounter == GUI_WIDTH / 8) 
            {
              xcounter = 0; 
              index = (index + 1) % 8; 
            }
            xcounter++;
            drawPixel(x, y, colors[index]);
        }
    }*/

    // -------------------------------------------------------------------------
    // Application logic
    // -------------------------------------------------------------------------
    while (true)
    {
        uart_rx_poll(&uart_rx);
        tight_loop_contents();
    }
}
