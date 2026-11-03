#include "../../kernel/kernel.h"
#include "timer.h"

#define PIT_COMMAND_PORT 0x43
#define PIT_CHANNEL0_PORT 0x40
#define PIT_BASE_FREQUENCY 1193182
#define PIT_FREQUENCY 100

void (*custom_timer_interrupt_handler)() = 0;
static volatile u32 timer_ticks = 0;

u32 timer_get_ticks(void) {
    return timer_ticks;
}

/* Configure PIT channel 0 for 100 interrupts per second (10 ms per tick). */
void init_timer(void) {
    u16 divisor = PIT_BASE_FREQUENCY / PIT_FREQUENCY;
    out(PIT_COMMAND_PORT, 0x36);
    out(PIT_CHANNEL0_PORT, divisor & 0xFF);
    out(PIT_CHANNEL0_PORT, (divisor >> 8) & 0xFF);
}

void timer_handler(__attribute__((unused)) u32 interrupt) {
    timer_ticks++;
    if (custom_timer_interrupt_handler != 0) {
        custom_timer_interrupt_handler();
    }
}

void register_timer_interrupt_handler(void) {
    set_interrupt_handler(INTERRUPT_TIMER, timer_handler);
}

void timer_set_handler(void (*handler)(void)) {
    custom_timer_interrupt_handler = handler;
}
