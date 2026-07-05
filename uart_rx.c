#include "uart_rx.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static void report_error(uart_rx_t *ctx, const char *raw_line, const char *reason)
{
    if (ctx->error_cb != NULL)
    {
        ctx->error_cb(raw_line, reason, ctx->user_ctx);
    }
}

static bool parse_u16_strict(const char *s, uint16_t *value)
{
    char *end = NULL;
    unsigned long v;
    const char *start = s;

    if ((s == NULL) || (*s == '\0'))
    {
        return false;
    }

    while (*s != '\0')
    {
        if (!isdigit((unsigned char)*s))
        {
            return false;
        }
        ++s;
    }

    v = strtoul(start, &end, 10);
    if ((end == NULL) || (*end != '\0') || (v > 65535UL))
    {
        return false;
    }

    *value = (uint16_t)v;
    return true;
}

static bool parse_u32_strict(const char *s, uint32_t *value)
{
    char *end = NULL;
    unsigned long v;
    const char *start = s;

    if ((s == NULL) || (*s == '\0'))
    {
        return false;
    }

    while (*s != '\0')
    {
        if (!isdigit((unsigned char)*s))
        {
            return false;
        }
        ++s;
    }

    v = strtoul(start, &end, 10);
    if ((end == NULL) || (*end != '\0'))
    {
        return false;
    }

    *value = (uint32_t)v;
    return true;
}

static void parse_line(uart_rx_t *ctx, char *line)
{
    char *first_colon;
    char *second_colon;
    uart_rx_message_t msg;
    uint16_t line_index;

    if ((line == NULL) || (*line == '\0'))
    {
        report_error(ctx, "", "empty line");
        return;
    }

    first_colon = strchr(line, ':');
    if (first_colon == NULL)
    {
        report_error(ctx, line, "missing first colon");
        return;
    }

    *first_colon = '\0';
    if (!parse_u16_strict(line, &line_index))
    {
        report_error(ctx, line, "invalid line index");
        return;
    }

    msg.line_index = line_index;

    if (line_index == 0U)
    {
        uint16_t number_of_lines;
        uint16_t font_height;
        char *payload = first_colon + 1;

        second_colon = strchr(payload, ':');
        if (second_colon == NULL)
        {
            report_error(ctx, payload, "header needs number_of_lines:font_height");
            return;
        }

        *second_colon = '\0';
        if (!parse_u16_strict(payload, &number_of_lines))
        {
            report_error(ctx, payload, "invalid number_of_lines");
            return;
        }

        if (!parse_u16_strict(second_colon + 1, &font_height))
        {
            report_error(ctx, second_colon + 1, "invalid font_height");
            return;
        }

        msg.type = UART_RX_LINE_TYPE_HEADER;
        msg.data.header.number_of_lines = number_of_lines;
        msg.data.header.font_height = font_height;
    }
    else
    {
        uint32_t color;
        char *payload = first_colon + 1;

        second_colon = strrchr(payload, ':');
        if ((second_colon == NULL) || (second_colon == payload))
        {
            report_error(ctx, payload, "text line needs text:color");
            return;
        }

        *second_colon = '\0';
        if (!parse_u32_strict(second_colon + 1, &color))
        {
            report_error(ctx, second_colon + 1, "invalid color");
            return;
        }

        msg.type = UART_RX_LINE_TYPE_TEXT;
        msg.data.text.text = payload;
        msg.data.text.color = color;
    }

    if (ctx->message_cb != NULL)
    {
        ctx->message_cb(&msg, ctx->user_ctx);
    }
}

void uart_rx_init(uart_rx_t *ctx,
                  uart_inst_t *uart,
                  char *rx_buffer,
                  size_t rx_buffer_size,
                  uart_rx_message_cb_t message_cb,
                  uart_rx_error_cb_t error_cb,
                  void *user_ctx)
{
    ctx->uart = uart;
    ctx->rx_buffer = rx_buffer;
    ctx->rx_buffer_size = rx_buffer_size;
    ctx->rx_length = 0U;
    ctx->message_cb = message_cb;
    ctx->error_cb = error_cb;
    ctx->user_ctx = user_ctx;

    if ((ctx->rx_buffer != NULL) && (ctx->rx_buffer_size > 0U))
    {
        ctx->rx_buffer[0] = '\0';
    }
}

void uart_rx_reset(uart_rx_t *ctx)
{
    ctx->rx_length = 0U;
    if ((ctx->rx_buffer != NULL) && (ctx->rx_buffer_size > 0U))
    {
        ctx->rx_buffer[0] = '\0';
    }
}

void uart_rx_poll(uart_rx_t *ctx)
{
    while (uart_is_readable(ctx->uart))
    {
        char ch = (char)uart_getc(ctx->uart);

        if (ch == '\r')
        {
            continue;
        }

        if (ch == '\n')
        {
            if (ctx->rx_length < ctx->rx_buffer_size)
            {
                ctx->rx_buffer[ctx->rx_length] = '\0';
                parse_line(ctx, ctx->rx_buffer);
            }
            else
            {
                report_error(ctx, "", "buffer overflow before newline");
            }
            uart_rx_reset(ctx);
            continue;
        }

        if (ctx->rx_length + 1U >= ctx->rx_buffer_size)
        {
            report_error(ctx, ctx->rx_buffer, "receive buffer too small");
            uart_rx_reset(ctx);
            continue;
        }

        ctx->rx_buffer[ctx->rx_length++] = ch;
    }
}
