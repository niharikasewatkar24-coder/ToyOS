#include <stdint.h>
#include <stdarg.h>
#include "sched.h"
static inline void outb(uint16_t p, uint8_t v){ __asm__ volatile("outb %0,%1"::"a"(v),"Nd"(p)); }
static void putc_(char c){ while(0); outb(0x3f8,c); }
static void puts_(const char*s){ while(*s) putc_(*s++); }
static void putu(uint32_t v){ char b[12]; int i=0; if(!v) b[i++]='0'; while(v){b[i++]='0'+v%10; v/=10;} while(i) putc_(b[--i]); }
void kprintf(const char *f, ...){
    va_list a; va_start(a,f);
    for(;*f;f++){
        if(*f!='%'){ putc_(*f); continue; }
        f++;
        if(*f=='s') puts_(va_arg(a,const char*));
        else if(*f=='u') putu(va_arg(a,uint32_t));
        else if(*f=='d'){ int v=va_arg(a,int); if(v<0){putc_('-'); v=-v;} putu(v); }
        else putc_(*f);
    }
    va_end(a);
}
struct idt_e { uint16_t lo, sel; uint8_t zero, flags; uint16_t hi; } __attribute__((packed));
static struct idt_e idt[256];
extern void irq0_stub(void);
void irq0_handler(void){ outb(0x20,0x20); sched_tick(); }
extern void run_all_tests(void);
void kernel_main(void){
    /* GRUB-less: QEMU -kernel boots with flat GDT, CS=0x08 */
    uint32_t h=(uint32_t)irq0_stub;
    idt[32].lo=h&0xffff; idt[32].hi=h>>16; idt[32].sel=0x08; idt[32].flags=0x8E;
    struct { uint16_t l; uint32_t b; } __attribute__((packed)) p={sizeof(idt)-1,(uint32_t)idt};
    __asm__ volatile("lidt %0"::"m"(p));
    /* remap PIC, unmask only IRQ0 */
    outb(0x20,0x11); outb(0xA0,0x11); outb(0x21,32); outb(0xA1,40);
    outb(0x21,4); outb(0xA1,2); outb(0x21,1); outb(0xA1,1);
    outb(0x21,0xFE); outb(0xA1,0xFF);
    uint32_t d=1193182/100; outb(0x43,0x36); outb(0x40,d&0xff); outb(0x40,d>>8);
    sched_init();
    kprintf("kernel up\n");
    run_all_tests();
    kprintf("ALL DONE\n");
    outb(0xf4,0);
    for(;;) __asm__ volatile("hlt");
}
