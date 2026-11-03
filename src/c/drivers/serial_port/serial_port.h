#ifndef SERIAL_H
#define SERIAL_H

#include "../../kernel/kernel.h"

enum log_level {
    LOG_INFO = 1,
    LOG_ERROR,
    LOG_WARNING
};

/* Configures the default COM1 serial port. */
extern void configure_default_serial_port(void);

/* Prints text to the default serial port. */
extern void serial_print(const char *str);

/* Writes structured JSON Lines events to the serial port. */
extern void serial_log(enum log_level level, const char *message);
extern void serial_log_event(enum log_level level, const char *event,
                             const char *message);
extern void serial_log_performance_sample(u32 tick, u32 switches,
                                          u32 running_task, u32 used_pages,
                                          u32 total_pages);

#endif
