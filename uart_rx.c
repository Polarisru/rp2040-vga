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

/* F:<color> */
static bool parse_cmd_fill(uart_rx_t *ctx, char *payload, uart_rx_message_t *msg)
{
    uint16_t color;
    if (!parse_u16_strict(payload, &color) || color > 7U)
    {
        report_error(ctx, payload, "F: invalid color (0..7)");
        return false;
    }
    msg->type        = UART_RX_CMD_FILL;
    msg->data.fill.color = (uint8_t)color;
    return true;
}

/* R:<x1>:<y1>:<x2>:<y2>:<color> */
static bool parse_cmd_rect(uart_rx_t *ctx, char *payload, uart_rx_message_t *msg)
{
    char *p = payload;
    char *tok;
    uint16_t fields[5];

    for (int i = 0; i < 5; i++)
    {
        tok = strchr(p, ':');
        if (i < 4 && tok == NULL) { report_error(ctx, payload, "R: too few fields"); return false; }
        if (tok != NULL) *tok = '\0';
        if (!parse_u16_strict(p, &fields[i])) { report_error(ctx, p, "R: invalid field"); return false; }
        if (tok != NULL) p = tok + 1;
    }

    if (fields[4] > 7U) { report_error(ctx, payload, "R: invalid color (0..7)"); return false; }

    msg->type           = UART_RX_CMD_RECT;
    msg->data.rect.x1   = fields[0];
    msg->data.rect.y1   = fields[1];
    msg->data.rect.x2   = fields[2];
    msg->data.rect.y2   = fields[3];
    msg->data.rect.color = (uint8_t)fields[4];
    return true;
}

/* T:<x>:<y>:<color>:<font>:<text> */
static bool parse_cmd_text(uart_rx_t *ctx, char *payload, uart_rx_message_t *msg)
{
    char *p = payload;
    char *tok;
    uint16_t nums[4];   /* x, y, color, font */

    for (int i = 0; i < 4; i++)
    {
        tok = strchr(p, ':');
        if (tok == NULL) { report_error(ctx, payload, "T: too few fields"); return false; }
        *tok = '\0';
        if (!parse_u16_strict(p, &nums[i])) { report_error(ctx, p, "T: invalid field"); return false; }
        p = tok + 1;
    }

    if (nums[2] > 7U) { report_error(ctx, payload, "T: invalid color (0..7)"); return false; }
    if (nums[3] > 2U) { report_error(ctx, payload, "T: invalid font (0..2)");  return false; }

    msg->type                 = UART_RX_CMD_TEXT;
    msg->data.draw_text.x     = nums[0];
    msg->data.draw_text.y     = nums[1];
    msg->data.draw_text.color = (uint8_t)nums[2];
    msg->data.draw_text.font  = (uint8_t)nums[3];
    msg->data.draw_text.text  = p;   /* remainder of buffer — valid until next poll byte */
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

    /* Single-letter commands: F, R, T */
    if ((line[0] == 'F' || line[0] == 'R' || line[0] == 'T') && line[1] == ':')
    {
        char cmd     = line[0];
        char *payload = line + 2;
        bool ok = false;

        if      (cmd == 'F') ok = parse_cmd_fill(ctx, payload, &msg);
        else if (cmd == 'R') ok = parse_cmd_rect(ctx, payload, &msg);
        else                 ok = parse_cmd_text(ctx, payload, &msg);

        if (ok && ctx->message_cb != NULL)
            ctx->message_cb(&msg, ctx->user_ctx);
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
