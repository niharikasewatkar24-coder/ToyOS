#include "../../kernel/kernel.h"
#include "timer.h"

#define PIT_COMMAND_PORT 0x43
#define PIT_CHANNEL0_PORT 0x40

#define PIT_BASE_FREQUENCY 1193182
#define PIT_FREQUENCY 100

void (*custom_timer_interrupt_handler)() = 0;

/*
 * Configure PIT channel 0 to generate timer interrupts
 * at a fixed frequency.
 *
 * PIT base frequency ≈ 1,193,182 Hz
 * Divisor = 1,193,182 / 100 ≈ 11,932
 *
 * Therefore:
 *     1 tick ≈ 10 ms
 *     100 ticks ≈ 1 second
 */
void init_timer() {
    u16 divisor = PIT_BASE_FREQUENCY / PIT_FREQUENCY;

    // Channel 0, access low byte then high byte,
    // mode 3 (square wave), binary mode.
    out(PIT_COMMAND_PORT, 0x36);

    // Send divisor low byte first, then high byte.
    out(PIT_CHANNEL0_PORT, divisor & 0xFF);
    out(PIT_CHANNEL0_PORT, (divisor >> 8) & 0xFF);
}

void timer_handler(__attribute__((unused)) u32 interrupt) {
    if (custom_timer_interrupt_handler != 0) {
        custom_timer_interrupt_handler();
    }
}

void register_timer_interrupt_handler() {
    set_interrupt_handler(INTERRUPT_TIMER, timer_handler);
}

void timer_set_handler(void (*handler)()) {
    custom_timer_interrupt_handler = handler;
}
