#include "kernel.h"
#include "../scheduler/scheduler.h"

#define INTERRUPT_GATE_TYPE_ATTRIBUTES 0x8E

#define MASTER_PIC_COMMAND_PORT 0x20
#define MASTER_PIC_DATA_PORT (MASTER_PIC_COMMAND_PORT + 1)

#define SLAVE_PIC_COMMAND_PORT 0xA0
#define SLAVE_PIC_DATA_PORT (SLAVE_PIC_COMMAND_PORT + 1)

#define MASTER_INTERRUPT_OFFSET 32
#define SLAVE_INTERRUPT_OFFSET (MASTER_INTERRUPT_OFFSET + 8)

#define END_OF_INTERRUPT_COMMAND 0x20
#define MODE_8086 0x01


extern void irq0();
extern void irq1();
extern void irq2();
extern void irq3();
extern void irq4();
extern void irq5();
extern void irq6();
extern void irq7();

extern void irq8();
extern void irq9();
extern void irq10();
extern void irq11();
extern void irq12();
extern void irq13();
extern void irq14();
extern void irq15();


void *irq_handlers[16] =
{
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0
};


void set_interrupt_handler(
    u32 interrupt,
    void (*handler)(u32 interrupt)
)
{
    irq_handlers[interrupt] = handler;
}


void idt_set_interrupt_handler(
    u8 interrupt,
    void (*handler_ptr)()
)
{
    set_idt_entry(
        interrupt,
        (u32)handler_ptr,
        KERNEL_CODE_SEGMENT,
        INTERRUPT_GATE_TYPE_ATTRIBUTES
    );
}


void init_interrupt_handlers()
{
    /*
     * Save the current PIC masks.
     */
    const u8 master_marks = in(MASTER_PIC_DATA_PORT);
    const u8 slave_marks = in(SLAVE_PIC_DATA_PORT);


    /*
     * Start PIC initialization.
     */
    out(MASTER_PIC_COMMAND_PORT, 0x11);
    out(SLAVE_PIC_COMMAND_PORT, 0x11);


    /*
     * Remap master PIC to interrupts 32-39.
     */
    out(
        MASTER_PIC_DATA_PORT,
        MASTER_INTERRUPT_OFFSET
    );


    /*
     * Remap slave PIC to interrupts 40-47.
     */
    out(
        SLAVE_PIC_DATA_PORT,
        SLAVE_INTERRUPT_OFFSET
    );


    /*
     * Tell master that slave is connected to IRQ2.
     */
    out(
        MASTER_PIC_DATA_PORT,
        0x04
    );


    /*
     * Tell slave its cascade identity.
     */
    out(
        SLAVE_PIC_DATA_PORT,
        0x02
    );


    /*
     * Set 8086 mode.
     */
    out(
        MASTER_PIC_DATA_PORT,
        MODE_8086
    );

    out(
        SLAVE_PIC_DATA_PORT,
        MODE_8086
    );


    /*
     * Restore PIC masks.
     */
    out(
        MASTER_PIC_DATA_PORT,
        master_marks
    );

    out(
        SLAVE_PIC_DATA_PORT,
        slave_marks
    );


    /*
     * Register IRQ handlers in the IDT.
     */
    idt_set_interrupt_handler(
        MASTER_INTERRUPT_OFFSET,
        irq0
    );

    idt_set_interrupt_handler(
        MASTER_INTERRUPT_OFFSET + 1,
        irq1
    );

    idt_set_interrupt_handler(
        MASTER_INTERRUPT_OFFSET + 2,
        irq2
    );

    idt_set_interrupt_handler(
        MASTER_INTERRUPT_OFFSET + 3,
        irq3
    );

    idt_set_interrupt_handler(
        MASTER_INTERRUPT_OFFSET + 4,
        irq4
    );

    idt_set_interrupt_handler(
        MASTER_INTERRUPT_OFFSET + 5,
        irq5
    );

    idt_set_interrupt_handler(
        MASTER_INTERRUPT_OFFSET + 6,
        irq6
    );

    idt_set_interrupt_handler(
        MASTER_INTERRUPT_OFFSET + 7,
        irq7
    );


    idt_set_interrupt_handler(
        SLAVE_INTERRUPT_OFFSET,
        irq8
    );

    idt_set_interrupt_handler(
        SLAVE_INTERRUPT_OFFSET + 1,
        irq9
    );

    idt_set_interrupt_handler(
        SLAVE_INTERRUPT_OFFSET + 2,
        irq10
    );

    idt_set_interrupt_handler(
        SLAVE_INTERRUPT_OFFSET + 3,
        irq11
    );

    idt_set_interrupt_handler(
        SLAVE_INTERRUPT_OFFSET + 4,
        irq12
    );

    idt_set_interrupt_handler(
        SLAVE_INTERRUPT_OFFSET + 5,
        irq13
    );

    idt_set_interrupt_handler(
        SLAVE_INTERRUPT_OFFSET + 6,
        irq14
    );

    idt_set_interrupt_handler(
        SLAVE_INTERRUPT_OFFSET + 7,
        irq15
    );
}


/*
 * Exact stack layout produced by int_handlers.asm:
 *
 * GS
 * FS
 * ES
 * DS
 *
 * EDI
 * ESI
 * EBP
 * ESP
 * EBX
 * EDX
 * ECX
 * EAX
 *
 * INTERRUPT
 *
 * EIP
 * CS
 * EFLAGS
 *
 * For a ring-0 interrupt there is no USERESP/SS.
 */
struct irq_stack_state
{
    u32 gs, fs, es, ds;

    /* Layout produced by PUSHA. */
   u32 edi, esi, ebp, esp;
u32 ebx, edx, ecx, eax;

    u32 interrupt;
    u32 eip, cs, eflags;
};

/*
 * Called by int_handlers.asm.
 *
 * Returns the context that the assembly should restore.
 */
u32 *kernel_interrupt_handler(
    struct irq_stack_state *stack_ptr
)
{
    /*
     * Determine which IRQ handler is registered.
     */
    u32 irq_number =
        stack_ptr->interrupt - MASTER_INTERRUPT_OFFSET;

    void (*handler)(u32 interrupt) =
        irq_handlers[irq_number];


    /*
     * Run the registered IRQ handler.
     */
    if (handler)
    {
        handler(stack_ptr->interrupt);
    }


    /*
     * Send EOI to the slave PIC when necessary.
     */
    if (
        stack_ptr->interrupt >=
        SLAVE_INTERRUPT_OFFSET
    )
    {
        out(
            SLAVE_PIC_COMMAND_PORT,
            END_OF_INTERRUPT_COMMAND
        );
    }


    /*
     * Always send EOI to the master PIC.
     */
    out(
        MASTER_PIC_COMMAND_PORT,
        END_OF_INTERRUPT_COMMAND
    );


    /*
     * Timer interrupt = IRQ0 = interrupt 32.
     *
     * Give the scheduler the current context.
     *
     * The scheduler returns either:
     *
     * 1. The same context
     * 2. A different task's context
     */
    if (
        stack_ptr->interrupt ==
        MASTER_INTERRUPT_OFFSET
    )
    {
        return scheduler_tick(
            (u32 *)stack_ptr
        );
    }


    /*
     * For non-timer interrupts, restore the
     * interrupted context.
     */
    return (u32 *)stack_ptr;
}
