#ifndef KERNEL_H
#define KERNEL_H

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned char bool;

#define true 1
#define false 0

// TODO: Ensure this has the same value as defined in GDT.
#define KERNEL_CODE_SEGMENT 0x08

#define INTERRUPT_TIMER 0
#define INTERRUPT_KEYBOARD 1

/**
 * Reads a single byte from the given port.
 */
extern u8 in(u16 port);

/**
 * Writes the given byte to the given port.
 */
extern void out(u16 port, u8 byte);

/**
 * Reads a 16-bit word from the given port.
 */
extern u16 inw(u16 port);

/**
 * Writes a 16-bit word to the given port.
 */
extern void outw(u16 port, u16 word);

/**
 * Enables interrupts.
 */
extern void enable_interrupts();
extern u32 irq_save(void);
extern void irq_restore(u32 flags);

/**
 * Halts the CPU until the next interrupt occurs.
 */
extern void halt();

/**
 * Initializes GDT.
 */
extern void init_gdt();

extern void set_kernel_stack(u32 stack);

/**
 * Initializes IDT.
 */
extern void init_idt();

/**
 * Sets the given function to be executed when the given interrupt occurs.
 */
extern void set_idt_entry(
    u8 interrupt,
    u32 base,
    u16 code_selector,
    u8 attributes
);

/**
 * Initializes interrupt requests (IRQs).
 */
extern void init_interrupt_handlers();

/**
 * Initializes exception handlers.
 */
extern void init_exception_handlers();

/**
 * Registers a handler for a given interrupt number (0–15).
 */
extern void set_interrupt_handler(
    u32 interrupt,
    void (*handler)(u32 interrupt)
);

/**
 * Registers a handler for exceptions.
 */
extern void set_exception_handler(
    void (*handler)(u32 interrupt, u32 error, const char *message)
);

#endif