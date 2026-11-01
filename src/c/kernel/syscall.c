

#include "kernel.h"
#include "../drivers/serial_port/serial_port.h"
#include "../memory/paging.h"
#include "../scheduler/scheduler.h"

#define SYS_WRITE 1
#define SYS_EXIT  2

#define USER_START 0x00400000
#define USER_END   0x00800000
#define MAX_LENGTH 127

struct syscall_frame
{
    u32 gs, fs, es, ds;
    u32 edi, esi, ebp, esp;
    u32 ebx, edx, ecx, eax;
    u32 eip, cs, eflags;
    u32 useresp, ss;
};

void syscall_handler(struct syscall_frame *frame)
{
    if (frame->eax == SYS_WRITE)
    {
        u32 address = frame->ebx;
        char buffer[MAX_LENGTH + 1];
        u32 i;

        if (address < USER_START || address >= USER_END)
        {
            frame->eax = (u32)-1;
            return;
        }

        for (i = 0; i < MAX_LENGTH; i++)
        {
            u32 current = address + i;

            if (current < address || current >= USER_END)
            {
                frame->eax = (u32)-1;
                return;
            }

            if (!paging_is_user_page(current))
            {
                frame->eax = (u32)-1;
                return;
            }

            buffer[i] = *((char *)current);

            if (buffer[i] == '\0')
                break;
        }

        if (i == MAX_LENGTH)
        {
            frame->eax = (u32)-1;
            return;
        }

        serial_print(buffer);
        frame->eax = 0;
    }
    else if (frame->eax == SYS_EXIT)
    {
        serial_log(LOG_INFO, "RING 3 EXIT CALLED");

        scheduler_terminate_current();

        frame->eax = 0;
    }
    else
    {
        frame->eax = (u32)-1;
    }
}
