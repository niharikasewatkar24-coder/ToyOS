// #include "kernel.h"

// #define GDT_SIZE 3

// /**
//  * Loads IDT pointing to the memory address given.
//  */
// extern void load_gdt(void *gdt_address);

// /*
// GDT is used to describe segments. Each segment is described using
// a segment descriptor, where segment descriptor is defined using 8 bytes.

// Bytes:
// 1. [0: 7]    - limit
// 2. [0: 7]    - limit
// 3. [0: 7]    - base
// 4. [0: 7]    - base
// 5. [0: 7]    - base
// 6. [0: 3]    - segment type
//       [4]    - descriptor type
//       [5: 6] - descriptor privilege level
//       [7]    - segment present
// 7. [0: 3]    - limit
//       [4]    - available for system use (set to 0)
//       [5]    - 64-bit code segment
//       [6]    - operand size (0 = 16bit, 1=32bit)
//       [7]    - granularity  (0 = 1b, 1=4kb)
// 8. [0: 7]    - base
//  */
// struct gdt_entry {
//     u16 limit01;
//     u16 base01;
//     u8 base2;
//     u8 fields5;
//     u8 fields6;
//     u8 base3;
// } __attribute__((packed));

// struct gdt_pointer {
//     u16 limit;
//     u32 base;
// } __attribute__((packed));

// struct gdt_entry gdt[GDT_SIZE];
// struct gdt_pointer gdt_ptr;

// void set_gdt_entry(int num, u32 base, u32 limit, u8 fields5, u8 fields6_47) {
//     gdt[num].base01 = (base & 0xFFFF);
//     gdt[num].base2 = (base >> 16) & 0xFF;
//     gdt[num].base3 = (base >> 24) & 0xFF;
//     gdt[num].limit01 = (limit & 0xFFFF);
//     gdt[num].fields5 = fields5;
//     gdt[num].fields6 = ((limit >> 16) & 0x0F);
//     gdt[num].fields6 |= (fields6_47 & 0xF0);
// }

// void init_gdt() {
//     gdt_ptr.limit = (sizeof(struct gdt_entry) * GDT_SIZE) - 1;
//     gdt_ptr.base = (u32) &gdt;

//     // null descriptor
//     set_gdt_entry(0, 0, 0, 0, 0);

//     // kernel code segment
//     set_gdt_entry(
//         1,          // second entry
//         0,          // base is 0
//         0xFFFFFFFF, // limit is 4gb
//         0b10011010, // 1010 - code (execute-read), 1 - (code/data), 00 - ring 0, 1 - present
//         0b11001111  // 1111 - limit (will be ignored, hence does not matter), 0 - AVL, 0 - not 64bit, 1 - 32bit, 1 - granularity 4k
//     );

//     // kernel data segment
//     set_gdt_entry(
//         2,          // third entry
//         0,          // base is 0
//         0xFFFFFFFF, // limit is 4gb
//         0b10010010, // 0010 - data (read-write), 1 - (code/data), 00 - ring 0, 1 - present
//         0b11001111  // 1111 - limit (will be ignored, hence does not matter), 0 - AVL, 0 - not 64bit, 1 - 32bit, 1 - granularity 4k
//     );

//     load_gdt(&gdt_ptr);
// }




#include "kernel.h"

#define GDT_SIZE 6

extern void load_gdt(void *gdt_address);
extern void load_tss(u16 selector);

/* GDT descriptor */
struct gdt_entry {
    u16 limit01;
    u16 base01;
    u8 base2;
    u8 fields5;
    u8 fields6;
    u8 base3;
} __attribute__((packed));

/* GDT pointer */
struct gdt_pointer {
    u16 limit;
    u32 base;
} __attribute__((packed));

/* Task State Segment */
struct tss_entry {
    u32 prev_tss;
    u32 esp0;
    u32 ss0;
    u32 esp1, ss1;
    u32 esp2, ss2;
    u32 cr3;
    u32 eip, eflags;
    u32 eax, ecx, edx, ebx;
    u32 esp, ebp, esi, edi;
    u32 es, cs, ss, ds, fs, gs;
    u32 ldt;
    u16 trap;
    u16 iomap_base;
} __attribute__((packed));

struct gdt_entry gdt[GDT_SIZE];
struct gdt_pointer gdt_ptr;

static struct tss_entry tss;

/* Temporary kernel stack for Ring 3 to Ring 0 transitions */
static u8 tss_stack[8192] __attribute__((aligned(16)));

/* Set one GDT descriptor */
void set_gdt_entry(
    int num,
    u32 base,
    u32 limit,
    u8 fields5,
    u8 fields6_47
) {
    gdt[num].base01 = base & 0xFFFF;
    gdt[num].base2 = (base >> 16) & 0xFF;
    gdt[num].base3 = (base >> 24) & 0xFF;

    gdt[num].limit01 = limit & 0xFFFF;

    gdt[num].fields5 = fields5;

    gdt[num].fields6 = (limit >> 16) & 0x0F;
    gdt[num].fields6 |= fields6_47 & 0xF0;
}

/* Initialize GDT and TSS */
void init_gdt(void) {

    gdt_ptr.limit = sizeof(struct gdt_entry) * GDT_SIZE - 1;
    gdt_ptr.base = (u32)&gdt;

    // Null descriptor
    set_gdt_entry(0, 0, 0, 0, 0);

    // Kernel code: Ring 0
    set_gdt_entry(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);

    // Kernel data: Ring 0
    set_gdt_entry(2, 0, 0xFFFFFFFF, 0x92, 0xCF);

    // User code: Ring 3
    set_gdt_entry(3, 0, 0xFFFFFFFF, 0xFA, 0xCF);

    // User data: Ring 3
    set_gdt_entry(4, 0, 0xFFFFFFFF, 0xF2, 0xCF);

    // Clear TSS
    u8 *p = (u8 *)&tss;

    for (u32 i = 0; i < sizeof(tss); i++) {
        p[i] = 0;
    }

    // Kernel stack used when entering Ring 0
    tss.ss0 = 0x10;
    tss.esp0 = (u32)(tss_stack + sizeof(tss_stack));

    // Disable user I/O port access
    tss.iomap_base = sizeof(tss);

    // TSS descriptor: GDT index 5
    set_gdt_entry(
        5,
        (u32)&tss,
        sizeof(tss) - 1,
        0x89,
        0x00
    );

    // Load GDT
    load_gdt(&gdt_ptr);

    // Load Task Register
    load_tss(0x28);
}
// Update kernel stack for the current task
void set_kernel_stack(u32 stack)
{
    tss.esp0 = stack;
}
