
[BITS 32]

global user_program_start
global user_program_end

user_program_start:

    ; SYS_WRITE: print message
    mov eax, 1
    call get_message

get_message:
    pop ebx
    add ebx, user_message - get_message
    int 0x80

    ; SYS_EXIT: terminate user task
    mov eax, 2
    int 0x80

hang:
    jmp hang

user_message:
    db "Hello from Ring 3!", 0

user_program_end: