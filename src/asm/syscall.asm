[BITS 32]

global syscall_entry
extern syscall_handler

syscall_entry:
    cld

    ; Save general-purpose registers.
    pusha

    ; Save segment registers.
    push ds
    push es
    push fs
    push gs

    ; Switch to kernel data segments.
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Pass saved register frame to C handler.
    mov eax, esp
    push eax
    call syscall_handler
    add esp, 4

    ; Restore registers.
    pop gs
    pop fs
    pop es
    pop ds

    popa

    ; Return to interrupted program.
    iret