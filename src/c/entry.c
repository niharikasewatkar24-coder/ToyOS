

#include "drivers/ide/ide.h"
#include "kernel/kernel.h"
#include "drivers/keyboard/keyboard.h"
#include "drivers/timer/timer.h"
#include "drivers/serial_port/serial_port.h"
#include "scheduler/scheduler.h"
#include "sync/mutex.h"
#include "memory/pmm.h"
#include "memory/paging.h"

/* Forward declarations */
void better_put_cursor(int x, int y);
void saveFramebufferState(void);
void restoreFramebufferState(void);
void draw_char(int x, int y, char ch);
void print_char(char symbol);
void print_void(void);
void print_message(char *message);
void print_text(char *message);
void shiftLines(int numOfLines);
void execute_command(char *command);
void key_handler(struct keyboard_event event);
void key_handler_two(struct keyboard_event event);
void timer_tick_handler(void);

void low_priority_task(void);
void medium_priority_task(void);
void high_priority_task(void);

/* Ring 3 program symbols */
extern unsigned char user_program_start[];
extern unsigned char user_program_end[];

/* Include source files */
#include "functions.c"
#include "commands.c"

bool screenSaverActive = false;

char savedFramebuffer[FRAMEBUFFER_SIZE];
char *framebuffer = (char *)0xb8000;

/* Priority inheritance demonstration */
static mutex_t priority_test_mutex;

static volatile u32 low_has_lock = 0;
static volatile u32 low_finished = 0;

static u32 medium_task_id = 0;
static u32 high_task_id = 0;

/* Exception handler */
void exception_handler(
    u32 interrupt,
    u32 error,
    const char *message
)
{
    (void)interrupt;
    (void)error;

    serial_log(LOG_ERROR, message);
}

/* Kernel initialization */
void init_kernel()
{
    init_gdt();
    init_idt();

    init_exception_handlers();
    init_interrupt_handlers();

    extern void syscall_entry(void);

    set_idt_entry(
        0x80,
        (u32)syscall_entry,
        KERNEL_CODE_SEGMENT,
        0xEE
    );

    init_timer();

    register_timer_interrupt_handler();
    register_keyboard_interrupt_handler();

    configure_default_serial_port();

    set_exception_handler(exception_handler);

    pmm_init();
    paging_init();
    scheduler_init();
}

/* LOW PRIORITY TASK */
void low_priority_task(void)
{
    serial_log(LOG_INFO, "LOW TASK START");

    mutex_lock(&priority_test_mutex);

    low_has_lock = 1;

    serial_log(LOG_INFO, "LOW HOLDS MUTEX");

    scheduler_wake_task(medium_task_id);
    scheduler_wake_task(high_task_id);

    volatile u32 work = 0;

    for (u32 i = 0; i < 50000000; i++)
    {
        work++;
    }

    (void)work;

    serial_log(LOG_INFO, "LOW RELEASING MUTEX");

    mutex_unlock(&priority_test_mutex);

    low_finished = 1;

    serial_log(LOG_INFO, "LOW FINISHED");

    scheduler_terminate_current();

    while (1)
    {
        halt();
    }
}

/* MEDIUM PRIORITY TASK */
void medium_priority_task(void)
{
    serial_log(LOG_INFO, "MEDIUM TASK START");
    serial_log(LOG_INFO, "MEDIUM RUNNING");

    while (!low_finished)
    {
        volatile u32 work = 0;

        for (u32 i = 0; i < 1000000; i++)
        {
            work++;
        }

        (void)work;
    }

    serial_log(LOG_INFO, "MEDIUM FINISHED");

    scheduler_terminate_current();

    while (1)
    {
        halt();
    }
}

/* HIGH PRIORITY TASK */
void high_priority_task(void)
{
    serial_log(LOG_INFO, "HIGH TASK START");
    serial_log(LOG_INFO, "HIGH TRYING MUTEX");

    mutex_lock(&priority_test_mutex);

    serial_log(LOG_INFO, "HIGH ACQUIRED MUTEX");

    mutex_unlock(&priority_test_mutex);

    serial_log(LOG_INFO, "HIGH FINISHED");

    scheduler_terminate_current();

    while (1)
    {
        halt();
    }
}

/* Move hardware cursor */
void put_cursor(unsigned short pos)
{
    out(0x3D4, 14);
    out(0x3D5, ((pos >> 8) & 0x00FF));

    out(0x3D4, 15);
    out(0x3D5, pos & 0x00FF);
}

/* Halt CPU forever */
_Noreturn void halt_loop()
{
    while (1)
    {
        halt();
    }
}

/* Position cursor */
void better_put_cursor(int x, int y)
{
    int idx = y * SCREEN_WIDTH + x;
    put_cursor(idx);
}

/* Save framebuffer */
void saveFramebufferState()
{
    for (int i = 0; i < FRAMEBUFFER_SIZE; ++i)
    {
        savedFramebuffer[i] = framebuffer[i];
    }

    savedSymbols_written = symbols_written;
    savedLines_written = lines_written;
}

/* Restore framebuffer */
void restoreFramebufferState()
{
    for (int i = 0; i < FRAMEBUFFER_SIZE; ++i)
    {
        framebuffer[i] = savedFramebuffer[i];
    }

    symbols_written = savedSymbols_written;
    lines_written = savedLines_written;

    better_put_cursor(
        symbols_written,
        lines_written
    );
}

/* Draw one character */
void draw_char(int x, int y, char ch)
{
    int idx = y * (SCREEN_WIDTH * 2) + (x * 2);

    framebuffer[idx] = ch;
    framebuffer[idx + 1] = (0x0 << 4) | 0x2;
}

/* Print character */
void print_char(char symbol)
{
    if (symbol != '\n')
    {
        draw_char(
            symbols_written,
            lines_written,
            symbol
        );
    }

    if (
        lines_written == (SCREEN_HEIGHT - 1)
        &&
        symbol == '\n'
    )
    {
        shiftLines(1);
        symbols_written = 0;
    }
    else if (
        (
            symbols_written >= SCREEN_WIDTH
            ||
            symbol == '\n'
        )
        &&
        lines_written < SCREEN_HEIGHT
    )
    {
        lines_written += 1;
        symbols_written = 0;
    }
    else
    {
        symbols_written++;
    }

    better_put_cursor(
        symbols_written,
        lines_written
    );
}

/* Delete previous character */
void print_void()
{
    if (symbols_written > 0)
    {
        symbols_written--;
    }

    draw_char(
        symbols_written,
        lines_written,
        ' '
    );

    better_put_cursor(
        symbols_written,
        lines_written
    );
}

/* Print message with newline */
void print_message(char *message)
{
    for (int i = 0; message[i] != '\0'; i++)
    {
        print_char(message[i]);
    }

    print_char('\n');
}

/* Print text without automatic newline */
void print_text(char *message)
{
    for (int i = 0; message[i] != '\0'; i++)
    {
        print_char(message[i]);
    }
}

/* Shift console lines upward */
void shiftLines(int numOfLines)
{
    for (int line = 0; line < numOfLines; line++)
    {
        for (
            int i = 0;
            i < SCREEN_WIDTH * (SCREEN_HEIGHT - 1) * 2;
            i++
        )
        {
            framebuffer[i] =
                framebuffer[i + (SCREEN_WIDTH * 2)];
        }

        for (
            int i = SCREEN_WIDTH * (SCREEN_HEIGHT - 1) * 2;
            i < SCREEN_WIDTH * SCREEN_HEIGHT * 2;
            i += 2
        )
        {
            framebuffer[i] = ' ';
            framebuffer[i + 1] = (char)((0x0 << 4) | 0xb);
        }
    }
}

/* Keyboard handler */
void key_handler(struct keyboard_event event)
{
    static char input_buffer[200];
    static int buffer_index = 0;

    if (
        screenSaverActive &&
        event.type == EVENT_KEY_PRESSED
    )
    {
        screenSaverActive = false;

        clear();
        restoreFramebufferState();

        return;
    }

    if (!screenSaverActive)
    {
        if (
            event.key_character &&
            event.type == EVENT_KEY_PRESSED
        )
        {
            if (event.key == KEY_ENTER)
            {
                input_buffer[buffer_index] = '\0';

                print_char('\n');

                execute_command(input_buffer);

                for (int i = 0; i < MAX_COMMAND_LENGTH; i++)
                {
                    input_buffer[i] = 0;
                }

                buffer_index = 0;
            }
            else if (event.key == KEY_BACKSPACE)
            {
                if (symbols_written > 0)
                {
                    print_void();

                    if (buffer_index > 0)
                    {
                        buffer_index--;
                    }
                }
            }
            else
            {
                if (symbols_written < SCREEN_WIDTH)
                {
                    char input_character = event.key_character;

                    print_char(input_character);

                    if (buffer_index < sizeof(input_buffer) - 1)
                    {
                        input_buffer[buffer_index] = input_character;
                        buffer_index++;
                    }
                }
            }
        }
    }
}

/* File editor keyboard handler */
void key_handler_two(struct keyboard_event event)
{
    if (
        event.key_character &&
        event.type == EVENT_KEY_PRESSED
    )
    {
        /* Press '-' to save and exit */
        if (event.key == KEY_MINUS)
        {
            files[index_of_file].content[file_index] = '\0';

            if (filesystem_save() == 0)
            {
                serial_log(LOG_INFO, "FILE SAVED TO DISK");
                print_message("File saved successfully");
            }
            else
            {
                serial_log(LOG_ERROR, "FILE SAVE FAILED");
                print_message("File save failed");
            }

            restoreFramebufferState();

            keyboard_set_handler(key_handler);
        }
        else if (event.key == KEY_ENTER)
        {
            print_char('\n');

            if (
                file_index <
                sizeof(files[index_of_file].content) - 1
            )
            {
                files[index_of_file].content[file_index] = '\n';
                file_index++;
            }
        }
        else if (event.key == KEY_BACKSPACE)
        {
            if (symbols_written > 0)
            {
                print_void();

                if (file_index > 0)
                {
                    file_index--;

                    files[index_of_file].content[file_index] = '\0';
                }
            }
        }
        else
        {
            if (symbols_written < SCREEN_WIDTH)
            {
                char input_character = event.key_character;

                print_char(input_character);

                if (
                    file_index <
                    sizeof(files[index_of_file].content) - 1
                )
                {
                    files[index_of_file].content[file_index] =
                        input_character;

                    file_index++;
                }
            }
        }
    }
}

/* Screen saver state */
int ballX = SCREEN_WIDTH / 2;
int ballY = SCREEN_HEIGHT / 2;

int velocityX = 1;
int velocityY = 1;

/* Timer handler */
void timer_tick_handler()
{
    if (screenSaverActive)
    {
        draw_char(ballX, ballY, ' ');

        ballX += velocityX;
        ballY += velocityY;

        if (ballX < 0 || ballX >= SCREEN_WIDTH)
        {
            velocityX = -velocityX;
            ballX += velocityX;
        }

        if (ballY < 0 || ballY >= SCREEN_HEIGHT)
        {
            velocityY = -velocityY;
            ballY += velocityY;
        }

        draw_char(ballX, ballY, 'O');

        better_put_cursor(ballX, ballY);
    }
}

/* Execute shell command */
void execute_command(char *command)
{
    char command_name[MAX_COMMAND_LENGTH];
    char argument[MAX_COMMAND_LENGTH];

    split_string(
        command,
        command_name,
        argument
    );

    for (
        int i = 0;
        i < sizeof(command_table) / sizeof(command_table[0]);
        ++i
    )
    {
        if (
            areStringsEqual(
                command_name,
                command_table[i].name
            )
        )
        {
            if (
                command_table[i].requires_argument &&
                argument[0] == '\0'
            )
            {
                print_message("Command requires an argument");
            }
            else
            {
                command_table[i].function(argument);
            }

            return;
        }
    }

    print_message("Command not found");
}

/* Kernel entry point */
void kernel_entry()
{
    init_kernel();

    /* Disable interrupts during setup */
    asm volatile ("cli");

    /* IDE disk read test */
    serial_log(LOG_INFO, "IDE DISK TEST STARTED");

    u16 disk_buffer[256];

    if (ide_read_sector(0, disk_buffer) == 0)
    {
        serial_log(LOG_INFO, "IDE SECTOR READ SUCCESSFUL");
    }
    else
    {
        serial_log(LOG_ERROR, "IDE SECTOR READ FAILED");
    }

    /* Initialize mutex */
    mutex_init(&priority_test_mutex);

    /* Create LOW task */
    task_create(low_priority_task, 2);

    /* Create MEDIUM task */
    medium_task_id = task_create(medium_priority_task, 1);

    /* Create HIGH task */
    high_task_id = task_create(high_priority_task, 0);

    /* Block MEDIUM and HIGH initially */
    scheduler_block_task(medium_task_id);
    scheduler_block_task(high_task_id);

    /* Setup Ring 3 user program */
    void *user_code_page = pmm_alloc_page();
    void *user_stack_page = pmm_alloc_page();

    if (user_code_page == 0 || user_stack_page == 0)
    {
        serial_log(LOG_ERROR, "User memory allocation failed");
    }
    else
    {
        serial_log(LOG_INFO, "User memory allocated");

        int code_mapped = paging_map_user_page((u32)user_code_page);
        int stack_mapped = paging_map_user_page((u32)user_stack_page);

        if (!code_mapped || !stack_mapped)
        {
            serial_log(LOG_ERROR, "User page mapping failed");
        }
        else
        {
            serial_log(LOG_INFO, "User pages mapped");

            u32 code_size =
                (u32)user_program_end - (u32)user_program_start;

            if (code_size == 0 || code_size > PAGE_SIZE)
            {
                serial_log(LOG_ERROR, "Invalid user program size");
            }
            else
            {
                unsigned char *dest =
                    (unsigned char *)user_code_page;

                for (u32 i = 0; i < code_size; i++)
                {
                    dest[i] = user_program_start[i];
                }

                serial_log(LOG_INFO, "User program copied");

                int user_task_id = task_create_user(
                    (u32)user_code_page,
                    (u32)user_stack_page + PAGE_SIZE,
                    0
                );

                if (user_task_id >= 0)
                {
                    serial_log(LOG_INFO, "Ring 3 task created");
                }
                else
                {
                    serial_log(LOG_ERROR, "Ring 3 task creation failed");
                }
            }
        }
    }

    /* Install keyboard handler */
    keyboard_set_handler(key_handler);

    /* Install timer callback */
    timer_set_handler(timer_tick_handler);

    /* Initialize filesystem */
    init_files();

    /* Clear console */
    clear();

    /* VGA display test */
    print_message("TOY OS VGA DISPLAY WORKING!");
    print_message("Ring 3 and scheduler test starting...");

    /* Set cursor to current position */
    better_put_cursor(
        symbols_written,
        lines_written
    );

    /* Enable interrupts */
    enable_interrupts();

    /* Keep bootstrap execution alive */
    halt_loop();
}
