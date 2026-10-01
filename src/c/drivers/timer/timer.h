#ifndef TIMER_H
#define TIMER_H

/**
 * Initializes the PIT to generate timer interrupts
 * at a fixed frequency.
 */
extern void init_timer();

/**
 * Registers timer interrupt handler that delegates handling
 * of the interrupt to the registered interrupt handler.
 */
extern void register_timer_interrupt_handler();

/**
 * Sets the given function as the handler of the timer interrupts.
 */
extern void timer_set_handler(void (*handler)());

#endif
