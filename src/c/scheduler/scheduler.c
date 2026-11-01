#include "scheduler.h"
#include "../drivers/serial_port/serial_port.h"
#include "../memory/pmm.h"
#include "../memory/paging.h"

/* Maximum number of tasks. */
static task_t tasks[MAX_TASKS];

/* Number of tasks currently created. */
static u32 task_count = 0;

/* Index of currently running task. */
static u32 current_task = 0;
static int scheduler_started = 0;

/* Scheduler reschedule request. */
static volatile u32 reschedule_requested = 0;

/*
 * Time slices for MLFQ queues.
 * Queue 0 = highest priority
 * Queue 1 = medium
 * Queue 2 = lowest priority
 */
static const u32 queue_time_slices[MLFQ_LEVELS] =
{
    1, 2, 4
};

/* Return time slice belonging to a queue level. */
static u32 get_time_slice(u32 queue_level)
{
    if (queue_level >= MLFQ_LEVELS)
        queue_level = MLFQ_LEVELS - 1;

    return queue_time_slices[queue_level];
}

/* Find the next READY task. */
static int find_next_task(void)
{
    if (task_count == 0)
        return -1;

    int best_task = -1;
    u32 best_queue = MLFQ_LEVELS;

    for (u32 offset = 1; offset <= task_count; offset++)
    {
        u32 index = (current_task + offset) % task_count;

        if (tasks[index].state == TASK_READY)
        {
            if (tasks[index].queue_level < best_queue)
            {
                best_queue = tasks[index].queue_level;
                best_task = (int)index;
            }
        }
    }

    if (best_task == -1)
    {
        for (u32 i = 0; i < task_count; i++)
        {
            if (tasks[i].state == TASK_READY)
            {
                if (tasks[i].queue_level < best_queue)
                {
                    best_queue = tasks[i].queue_level;
                    best_task = (int)i;
                }
            }
        }
    }

    return best_task;
}

/* Initialize scheduler. */
void scheduler_init(void)
{
    task_count = 0;
    current_task = 0;
    reschedule_requested = 0;
    scheduler_started = 0;

    for (u32 i = 0; i < MAX_TASKS; i++)
    {
        tasks[i].id = i;
        tasks[i].state = TASK_UNUSED;

        tasks[i].priority = 2;
        tasks[i].base_priority = 2;
        tasks[i].effective_priority = 2;

        tasks[i].queue_level = 2;
        tasks[i].base_queue_level = 2;

        tasks[i].time_slice = get_time_slice(2);
        tasks[i].ticks_used = 0;

        tasks[i].entry = 0;
        tasks[i].stack = 0;
        tasks[i].user_stack = 0;
        tasks[i].context = 0;
    }

    serial_log(LOG_INFO, "SCHEDULER INITIALIZED");
}

/* Create a normal Ring 0 task. */
int task_create(task_entry_t entry, u32 priority)
{
    if (task_count >= MAX_TASKS)
    {
        serial_log(LOG_ERROR, "TASK CREATE FAILED: MAX TASKS");
        return -1;
    }

    if (entry == 0)
    {
        serial_log(LOG_ERROR, "TASK CREATE FAILED: NULL ENTRY");
        return -1;
    }

    if (priority >= MLFQ_LEVELS)
        priority = MLFQ_LEVELS - 1;

    u32 id = task_count;
    task_t *task = &tasks[id];

    void *stack = pmm_alloc_page();

    if (stack == 0)
    {
        serial_log(LOG_ERROR, "TASK CREATE FAILED: NO STACK");
        return -1;
    }

    task->id = id;
    task->state = TASK_READY;

    task->priority = priority;
    task->base_priority = priority;
    task->effective_priority = priority;

    task->queue_level = priority;
    task->base_queue_level = priority;

    task->time_slice = get_time_slice(task->queue_level);
    task->ticks_used = 0;

    task->entry = entry;
    task->stack = stack;
    task->user_stack = 0;

    /* Initial Ring 0 context: 16 words. */
    u32 *context = (u32 *)(
        (u32)stack + TASK_STACK_SIZE - (16 * sizeof(u32))
    );

    context[0] = 0x10;
    context[1] = 0x10;
    context[2] = 0x10;
    context[3] = 0x10;

    context[4] = 0;
    context[5] = 0;
    context[6] = 0;
    context[7] = 0;
    context[8] = 0;
    context[9] = 0;
    context[10] = 0;
    context[11] = 0;

    context[12] = 32;
    context[13] = (u32)entry;
    context[14] = 0x08;
    context[15] = 0x202;

    task->context = context;
    task_count++;

    serial_log(LOG_INFO, "TASK CREATED");

    return (int)id;
}

/* Create a Ring 3 user task. */
int task_create_user(u32 entry_address, u32 user_stack_top, u32 priority)
{
    if (task_count >= MAX_TASKS)
        return -1;

    if (entry_address < 0x00400000 ||
        entry_address >= 0x00800000 ||
        user_stack_top <= 0x00400000 ||
        user_stack_top > 0x00800000)
        return -1;

    if (!paging_is_user_page(entry_address) ||
        !paging_is_user_page(user_stack_top - 1))
        return -1;

    if (priority >= MLFQ_LEVELS)
        priority = MLFQ_LEVELS - 1;

    u32 id = task_count;
    task_t *task = &tasks[id];

    void *stack = pmm_alloc_page();

    if (stack == 0)
    {
        serial_log(LOG_ERROR, "USER TASK STACK FAILED");
        return -1;
    }

    task->id = id;
    task->state = TASK_READY;

    task->priority = priority;
    task->base_priority = priority;
    task->effective_priority = priority;

    task->queue_level = priority;
    task->base_queue_level = priority;

    task->time_slice = get_time_slice(priority);
    task->ticks_used = 0;

    task->entry = 0;
    task->stack = stack;
    task->user_stack = (void *)user_stack_top;

    /* Ring 3 context: 18 words. */
    u32 *context = (u32 *)(
        (u32)stack + TASK_STACK_SIZE - (18 * sizeof(u32))
    );

    context[0] = 0x23; /* GS */
    context[1] = 0x23; /* FS */
    context[2] = 0x23; /* ES */
    context[3] = 0x23; /* DS */

    context[4] = 0; /* EDI */
    context[5] = 0; /* ESI */
    context[6] = 0; /* EBP */
    context[7] = 0; /* ESP */
    context[8] = 0; /* EBX */
    context[9] = 0; /* EDX */
    context[10] = 0; /* ECX */
    context[11] = 0; /* EAX */

    context[12] = 32;          /* IRQ0 */
    context[13] = entry_address;
    context[14] = 0x1B;         /* User CS */
    context[15] = 0x202;        /* EFLAGS */
    context[16] = user_stack_top;
    context[17] = 0x23;         /* User SS */

    task->context = context;
    task_count++;

    serial_log(LOG_INFO, "RING 3 TASK CREATED");

    return (int)id;
}

/* Return current task ID. */
u32 scheduler_get_current_task_id(void)
{
    return current_task;
}

/* Block currently running task. */
void scheduler_block_current(void)
{
    if (task_count == 0)
        return;

    if (tasks[current_task].state == TASK_RUNNING)
    {
        tasks[current_task].state = TASK_BLOCKED;

        serial_log(LOG_INFO, "CURRENT TASK BLOCKED");
        reschedule_requested = 1;
    }
}
/* Terminate currently running task. */
void scheduler_terminate_current(void)
{
    if (task_count == 0)
        return;

    if (tasks[current_task].state == TASK_RUNNING)
    {
        tasks[current_task].state = TASK_TERMINATED;

        serial_log(LOG_INFO, "CURRENT TASK TERMINATED");

        reschedule_requested = 1;
    }
}

/* Block a specific task. */
void scheduler_block_task(u32 task_id)
{
    if (task_id >= task_count)
        return;

    if (tasks[task_id].state == TASK_READY ||
        tasks[task_id].state == TASK_RUNNING)
    {
        tasks[task_id].state = TASK_BLOCKED;

        serial_log(LOG_INFO, "TASK BLOCKED");
        reschedule_requested = 1;
    }
}

/* Wake a blocked task. */
void scheduler_wake_task(u32 task_id)
{
    if (task_id >= task_count)
        return;

    if (tasks[task_id].state == TASK_BLOCKED)
    {
        tasks[task_id].state = TASK_READY;

        tasks[task_id].time_slice =
            get_time_slice(tasks[task_id].queue_level);

        tasks[task_id].ticks_used = 0;

        serial_log(LOG_INFO, "TASK WOKEN");
        reschedule_requested = 1;
    }
}

/* Request rescheduling on next timer interrupt. */
void scheduler_request_reschedule(void)
{
    reschedule_requested = 1;
}

/* Voluntarily yield CPU. */
void scheduler_yield(void)
{
    reschedule_requested = 1;
}

/* Priority inheritance. */
void scheduler_boost_task(u32 task_id, u32 priority)
{
    if (task_id >= task_count)
        return;

    if (priority >= MLFQ_LEVELS)
        priority = MLFQ_LEVELS - 1;

    if (priority < tasks[task_id].effective_priority)
    {
        tasks[task_id].effective_priority = priority;
        tasks[task_id].priority = priority;
        tasks[task_id].queue_level = priority;

        tasks[task_id].time_slice = get_time_slice(priority);
        tasks[task_id].ticks_used = 0;

        serial_log(LOG_INFO, "PRIORITY INHERIT");
        reschedule_requested = 1;
    }
}

/* Restore original priority after mutex release. */
void scheduler_restore_priority(u32 task_id)
{
    if (task_id >= task_count)
        return;

    tasks[task_id].effective_priority =
        tasks[task_id].base_priority;

    tasks[task_id].priority =
        tasks[task_id].base_priority;

    tasks[task_id].queue_level =
        tasks[task_id].base_queue_level;

    tasks[task_id].time_slice =
        get_time_slice(tasks[task_id].queue_level);

    tasks[task_id].ticks_used = 0;

    serial_log(LOG_INFO, "PRIORITY RESTORED");
    reschedule_requested = 1;
}

/* Return effective priority. */
u32 scheduler_get_task_priority(u32 task_id)
{
    if (task_id >= task_count)
        return MLFQ_LEVELS - 1;

    return tasks[task_id].effective_priority;
}

/* Scheduler tick: called by IRQ0. */
u32 *scheduler_tick(u32 *current_context)
{
    if (task_count == 0)
        return current_context;

    /* Start the first READY task. */
    if (!scheduler_started)
    {
        int first = find_next_task();

        if (first >= 0)
        {
            current_task = (u32)first;

            tasks[current_task].state = TASK_RUNNING;
            tasks[current_task].ticks_used = 0;

            set_kernel_stack(
                (u32)tasks[current_task].stack + TASK_STACK_SIZE
            );

            scheduler_started = 1;
            reschedule_requested = 0;

            serial_log(LOG_INFO, "SCHEDULER STARTED");

            return tasks[current_task].context;
        }

        return current_context;
    }

    /* Save current task context. */
    tasks[current_task].context = current_context;

    if (tasks[current_task].state == TASK_READY)
        tasks[current_task].state = TASK_RUNNING;

    tasks[current_task].ticks_used++;

    /* MLFQ time slice expired. */
    if (tasks[current_task].ticks_used >=
        tasks[current_task].time_slice)
    {
        if (tasks[current_task].queue_level < MLFQ_LEVELS - 1)
        {
            tasks[current_task].queue_level++;
            serial_log(LOG_INFO, "MLFQ QUEUE DEMOTION");
        }

        tasks[current_task].time_slice =
            get_time_slice(tasks[current_task].queue_level);

        tasks[current_task].ticks_used = 0;
        reschedule_requested = 1;
    }

    /* Decide whether to switch tasks. */
    if (tasks[current_task].state == TASK_BLOCKED ||
        tasks[current_task].state == TASK_TERMINATED ||
        reschedule_requested)
    {
        int next = find_next_task();

        if (next >= 0)
        {
            if (tasks[current_task].state == TASK_RUNNING)
                tasks[current_task].state = TASK_READY;

            current_task = (u32)next;

            tasks[current_task].state = TASK_RUNNING;
            tasks[current_task].ticks_used = 0;

            set_kernel_stack(
                (u32)tasks[current_task].stack + TASK_STACK_SIZE
            );

            reschedule_requested = 0;

            serial_log(LOG_INFO, "TASK SWITCH");

            return tasks[current_task].context;
        }
    }

    return current_context;
}

