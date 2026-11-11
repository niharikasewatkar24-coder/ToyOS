#define kernel_entry normal_kernel_entry
#include "../entry.c"
#undef kernel_entry

static volatile u32 mlfq_progress[8];

static void mlfq_worker(u32 id, const char *pass_message)
{
    while (1)
    {
        mlfq_progress[id]++;

        if (timer_get_ticks() >= 500)
        {
            if (mlfq_progress[id] > 1000)
                serial_log(LOG_INFO, pass_message);
            else
                serial_log(LOG_ERROR, "MLFQ TASK STARVED");

            scheduler_terminate_current();

            while (1)
                halt();
        }
    }
}

static void mlfq_task_0(void) { mlfq_worker(0, "MLFQ TASK 0 PASS"); }
static void mlfq_task_1(void) { mlfq_worker(1, "MLFQ TASK 1 PASS"); }
static void mlfq_task_2(void) { mlfq_worker(2, "MLFQ TASK 2 PASS"); }
static void mlfq_task_3(void) { mlfq_worker(3, "MLFQ TASK 3 PASS"); }
static void mlfq_task_4(void) { mlfq_worker(4, "MLFQ TASK 4 PASS"); }
static void mlfq_task_5(void) { mlfq_worker(5, "MLFQ TASK 5 PASS"); }
static void mlfq_task_6(void) { mlfq_worker(6, "MLFQ TASK 6 PASS"); }
static void mlfq_task_7(void) { mlfq_worker(7, "MLFQ TASK 7 PASS"); }

void kernel_entry(void)
{
    init_kernel();
    timer_set_handler(timer_tick_handler);

    if (task_create(mlfq_task_0, 0) < 0 ||
        task_create(mlfq_task_1, 0) < 0 ||
        task_create(mlfq_task_2, 0) < 0 ||
        task_create(mlfq_task_3, 0) < 0 ||
        task_create(mlfq_task_4, 0) < 0 ||
        task_create(mlfq_task_5, 0) < 0 ||
        task_create(mlfq_task_6, 0) < 0 ||
        task_create(mlfq_task_7, 2) < 0)
    {
        serial_log(LOG_ERROR, "MLFQ TEST TASK CREATION FAILED");
    }

    serial_log(LOG_INFO, "MLFQ STRESS TEST STARTED");
    enable_interrupts();
    halt_loop();
}
