#include "serial_port.h"
#include "../../kernel/kernel.h"
#include "../timer/timer.h"

#define SERIAL_COM1_BASE                0x3F8
#define SERIAL_DATA_PORT(base)          (base)
#define SERIAL_FIFO_COMMAND_PORT(base)  (base + 2)
#define SERIAL_LINE_COMMAND_PORT(base)  (base + 3)
#define SERIAL_MODEM_COMMAND_PORT(base) (base + 4)
#define SERIAL_LINE_STATUS_PORT(base)   (base + 5)

void configure_default_serial_port() {
    out(SERIAL_LINE_COMMAND_PORT(SERIAL_COM1_BASE), 0x80);
    out(SERIAL_DATA_PORT(SERIAL_COM1_BASE), 0x03);
    out(SERIAL_LINE_COMMAND_PORT(SERIAL_COM1_BASE), 0x03);
    out(SERIAL_FIFO_COMMAND_PORT(SERIAL_COM1_BASE), 0xC7);
    out(SERIAL_MODEM_COMMAND_PORT(SERIAL_COM1_BASE), 0x0B);
}

int in_queue_state(u32 com) {
    return in(SERIAL_LINE_STATUS_PORT(com)) & 0x20;
}

void serial_print_char(char c) {
    while (in_queue_state(SERIAL_COM1_BASE) == 0);
    out(SERIAL_DATA_PORT(SERIAL_COM1_BASE), c);
}

void serial_print(const char *str) {
    while (*str) {
        serial_print_char(*str++);
    }
}

void serial_log(enum log_level level, const char *message) {
    serial_log_event(level, "kernel.log", message);
}

static void serial_print_json_string(const char *str) {
    serial_print_char('"');
    while (*str) {
        unsigned char c = (unsigned char)*str++;
        if (c == 34 || c == 92) {
            serial_print_char(92);
            serial_print_char((char)c);
        } else if (c == 10) {
            serial_print_char(92);
            serial_print_char('n');
        } else if (c == 13) {
            serial_print_char(92);
            serial_print_char('r');
        } else if (c == 9) {
            serial_print_char(92);
            serial_print_char('t');
        } else if (c >= 0x20) {
            serial_print_char((char)c);
        }
    }
    serial_print_char('"');
}

static void serial_print_u32(u32 value) {
    char digits[10];
    u32 count = 0;
    if (value == 0) {
        serial_print_char('0');
        return;
    }
    while (value != 0) {
        digits[count++] = (char)('0' + (value % 10));
        value /= 10;
    }
    while (count > 0) {
        serial_print_char(digits[--count]);
    }
}

static void serial_print_json_key(const char *key) {
    serial_print_char('"');
    serial_print(key);
    serial_print_char('"');
    serial_print_char(':');
}

static void serial_print_line_end(void) {
    serial_print_char(13);
    serial_print_char(10);
}

void serial_log_event(enum log_level level, const char *event,
                      const char *message) {
    u32 tick = timer_get_ticks();
    serial_print_char('{');
    serial_print_json_key("tick");
    serial_print_u32(tick);
    serial_print_char(',');
    serial_print_json_key("uptime_ms");
    serial_print_u32(tick * 10);
    serial_print_char(',');
    serial_print_json_key("level");
    serial_print_json_string(level == LOG_INFO ? "INFO" :
                             level == LOG_WARNING ? "WARNING" :
                             level == LOG_ERROR ? "ERROR" : "UNKNOWN");
    serial_print_char(',');
    serial_print_json_key("event");
    serial_print_json_string(event);
    serial_print_char(',');
    serial_print_json_key("message");
    serial_print_json_string(message);
    serial_print_char('}');
    serial_print_line_end();
}

void serial_log_performance_sample(u32 tick, u32 switches,
                                   u32 running_task, u32 used_pages,
                                   u32 total_pages) {
    serial_print_char('{');
    serial_print_json_key("level");
    serial_print_json_string("INFO");
    serial_print_char(',');
    serial_print_json_key("event");
    serial_print_json_string("performance.sample");
    serial_print_char(',');
    serial_print_json_key("tick");
    serial_print_u32(tick);
    serial_print_char(',');
    serial_print_json_key("uptime_ms");
    serial_print_u32(tick * 10);
    serial_print_char(',');
    serial_print_json_key("task_switches_total");
    serial_print_u32(switches);
    serial_print_char(',');
    serial_print_json_key("running_task");
    serial_print_u32(running_task);
    serial_print_char(',');
    serial_print_json_key("memory_used_pages");
    serial_print_u32(used_pages);
    serial_print_char(',');
    serial_print_json_key("memory_total_pages");
    serial_print_u32(total_pages);
    serial_print_char('}');
    serial_print_line_end();
}
