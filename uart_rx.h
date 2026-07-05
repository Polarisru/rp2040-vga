#ifndef UART_RX_H
#define UART_RX_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "hardware/uart.h"

typedef enum
{
    UART_RX_LINE_TYPE_HEADER = 0,
    UART_RX_LINE_TYPE_TEXT   = 1
} uart_rx_line_type_t;

typedef struct
{
    uart_rx_line_type_t type;
    uint16_t line_index;
    union
    {
        struct
        {
            uint16_t number_of_lines;
            uint16_t font_height;
        } header;
        struct
        {
            const char *text;
            uint32_t color;
        } text;
    } data;
} uart_rx_message_t;

typedef void (*uart_rx_message_cb_t)(const uart_rx_message_t *message, void *user_ctx);
typedef void (*uart_rx_error_cb_t)(const char *raw_line, const char *reason, void *user_ctx);

typedef struct
{
    uart_inst_t *uart;
    char *rx_buffer;
    size_t rx_buffer_size;
    size_t rx_length;
    uart_rx_message_cb_t message_cb;
    uart_rx_error_cb_t error_cb;
    void *user_ctx;
} uart_rx_t;

void uart_rx_init(uart_rx_t *ctx,
                  uart_inst_t *uart,
                  char *rx_buffer,
                  size_t rx_buffer_size,
                  uart_rx_message_cb_t message_cb,
                  uart_rx_error_cb_t error_cb,
                  void *user_ctx);

void uart_rx_poll(uart_rx_t *ctx);
void uart_rx_reset(uart_rx_t *ctx);

#endif
