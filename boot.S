.set MAGIC, 0x1BADB002
.set FLAGS, 0
.section .text
.align 4
.long MAGIC
.long FLAGS
.long -(MAGIC+FLAGS)
.section .bss
.align 16
stack_bottom: .skip 16384
stack_top:
.section .text
.global _start
_start:
    mov $stack_top, %esp
    call kernel_main
1:  cli
    hlt
    jmp 1b
.global irq0_stub
irq0_stub:
    pusha
    call irq0_handler
    popa
    iret
