#ifndef TIMER_H
#define TIMER_H

#include "../../kernel/kernel.h"

/* Initializes the PIT to generate timer interrupts at a fixed frequency. */
extern void init_timer(void);

/* Registers the timer interrupt handler. */
extern void register_timer_interrupt_handler(void);

/* Sets the function called for each timer interrupt. */
extern void timer_set_handler(void (*handler)(void));

/* Returns the number of PIT ticks since initialization (100 Hz). */
extern u32 timer_get_ticks(void);

#endif
