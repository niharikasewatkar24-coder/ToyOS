#include "scheduler.h"
#include "../drivers/serial_port/serial_port.h"
#include "../memory/pmm.h"
#include "../memory/paging.h"
#include "../drivers/timer/timer.h"

/* Maximum number of tasks. */
static task_t tasks[MAX_TASKS];
static u32 task_count = 0;
static u32 current_task = 0;
static u32 scheduler_switches = 0;
static int scheduler_started = 0;
static volatile u32 reschedule_requested = 0;
#define IDLE_TASK_ID MAX_TASKS

static u32 idle_stack[TASK_STACK_SIZE / sizeof(u32)]
    __attribute__((aligned(16)));

static u32 *idle_context = 0;
static int current_is_idle = 0;

static void idle_task(void)
{
    while (1)
        halt();
}

/* Time slices for MLFQ queues: highest, medium, lowest priority. */
static const u32 queue_time_slices[MLFQ_LEVELS] = { 1, 2, 4 };

static u32 get_time_slice(u32 queue_level)
{
    if (queue_level >= MLFQ_LEVELS)
        queue_level = MLFQ_LEVELS - 1;
    return queue_time_slices[queue_level];
}

static int find_next_task(void)
{
    if (task_count == 0)
        return -1;

    int best_task = -1;
    int best_inherited = -1;
    u32 best_priority = MLFQ_LEVELS;
    u32 best_queue = MLFQ_LEVELS;

    for (u32 offset = 1; offset <= MAX_TASKS; offset++)
    {
        u32 index = (current_task + offset) % MAX_TASKS;

        if (tasks[index].state != TASK_READY)
            continue;

        int inherited =
            tasks[index].effective_priority <
            tasks[index].base_priority;

        u32 priority = tasks[index].effective_priority;
        u32 queue = tasks[index].queue_level;

        if (best_task == -1 ||
            inherited > best_inherited ||
            (inherited == best_inherited &&
             inherited && priority < best_priority) ||
            (inherited == best_inherited &&
             (!inherited || priority == best_priority) &&
             queue < best_queue))
        {
            best_task = (int)index;
            best_inherited = inherited;
            best_priority = priority;
            best_queue = queue;
        }
    }

    return best_task;
}
static int find_free_task_slot(void)
{
    for (u32 i = 0; i < MAX_TASKS; i++)
    {
        if (tasks[i].state == TASK_UNUSED)
            return (int)i;
    }

    return -1;
}
/*
 * Reclaim terminated tasks whose kernel stacks
 * are no longer being used by the CPU.
 */
static void reap_terminated_tasks(int executing_task)
{
    for (u32 i = 0; i < MAX_TASKS; i++)
    {
        task_t *task = &tasks[i];

        if (task->state != TASK_TERMINATED)
            continue;

        if ((int)i == executing_task)
            continue;

        if (task->user_code_page != 0)
        {
            u32 address = (u32)task->user_code_page;

            if (paging_is_user_page(address) &&
                !paging_unmap_user_page(address))
            {
                serial_log(LOG_ERROR, "USER CODE UNMAP FAILED");
                continue;
            }

            pmm_free_page(task->user_code_page);
            task->user_code_page = 0;
        }

        if (task->user_stack_page != 0)
        {
            u32 address = (u32)task->user_stack_page;

            if (paging_is_user_page(address) &&
                !paging_unmap_user_page(address))
            {
                serial_log(LOG_ERROR, "USER STACK UNMAP FAILED");
                continue;
            }

            pmm_free_page(task->user_stack_page);
            task->user_stack_page = 0;
        }

        if (task->stack != 0)
        {
            pmm_free_page(task->stack);
            task->stack = 0;
        }

        task->user_stack = 0;
        task->entry = 0;
        task->context = 0;

        task->priority = 2;
        task->base_priority = 2;
        task->effective_priority = 2;
        task->queue_level = 2;
        task->base_queue_level = 2;
        task->time_slice = get_time_slice(2);
        task->ticks_used = 0;

        task->state = TASK_UNUSED;

        if (task_count > 0)
            task_count--;

        serial_log(LOG_INFO, "TERMINATED TASK RECLAIMED");
    }
}
void scheduler_init(void)
{
    task_count = 0;
    current_task = 0;
    scheduler_switches = 0;
    reschedule_requested = 0;
    scheduler_started = 0;
    current_is_idle = 0;

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
        tasks[i].user_code_page = 0;
        tasks[i].user_stack_page = 0;
        tasks[i].context = 0;
    }
    idle_context = (u32 *)(
        (u32)idle_stack + TASK_STACK_SIZE -
        (16 * sizeof(u32))
    );

    for (u32 i = 0; i < 16; i++)
        idle_context[i] = 0;

    idle_context[0] = 0x10;
    idle_context[1] = 0x10;
    idle_context[2] = 0x10;
    idle_context[3] = 0x10;
    idle_context[12] = 32;
    idle_context[13] = (u32)idle_task;
    idle_context[14] = 0x08;
    idle_context[15] = 0x202;
    serial_log(LOG_INFO, "SCHEDULER INITIALIZED");
}

int task_create(task_entry_t entry, u32 priority)
{
    int free_slot = find_free_task_slot();

    if (free_slot < 0)
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

    u32 id = (u32)free_slot;
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
    task->user_code_page = 0;
    task->user_stack_page = 0;

    u32 *context = (u32 *)((u32)stack + TASK_STACK_SIZE - (16 * sizeof(u32)));
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

int task_create_user(u32 entry_address, u32 user_stack_top, u32 priority)
{
    int free_slot = find_free_task_slot();

    if (free_slot < 0)
        return -1;
    if (entry_address < 0x00400000 || entry_address >= 0x00800000 ||
        user_stack_top <= 0x00400000 || user_stack_top > 0x00800000)
        return -1;
    if (!paging_is_user_page(entry_address) ||
        !paging_is_user_page(user_stack_top - 1))
        return -1;
    if (priority >= MLFQ_LEVELS)
        priority = MLFQ_LEVELS - 1;

    u32 id = (u32)free_slot;
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
    task->user_code_page = (void *)(entry_address & ~(PAGE_SIZE - 1));
    task->user_stack_page = (void *)((user_stack_top - 1) & ~(PAGE_SIZE - 1));

    u32 *context = (u32 *)((u32)stack + TASK_STACK_SIZE - (18 * sizeof(u32)));
    context[0] = 0x23;
    context[1] = 0x23;
    context[2] = 0x23;
    context[3] = 0x23;
    context[4] = 0;
    context[5] = 0;
    context[6] = 0;
    context[7] = 0;
    context[8] = 0;
    context[9] = 0;
    context[10] = 0;
    context[11] = 0;
    context[12] = 32;
    context[13] = entry_address;
    context[14] = 0x1B;
    context[15] = 0x202;
    context[16] = user_stack_top;
    context[17] = 0x23;
    task->context = context;
    task_count++;
    serial_log(LOG_INFO, "RING 3 TASK CREATED");
    return (int)id;
}

u32 scheduler_get_current_task_id(void)
{
    return current_task;
}

void scheduler_block_current(void)
{
    if (task_count == 0 ||
        current_task >= MAX_TASKS ||
        tasks[current_task].state != TASK_RUNNING)
        return;
    if (tasks[current_task].state == TASK_RUNNING)
    {
        tasks[current_task].state = TASK_BLOCKED;
        serial_log(LOG_INFO, "CURRENT TASK BLOCKED");
        reschedule_requested = 1;
    }
}

void scheduler_terminate_current(void)
{
    if (task_count == 0 ||
        current_task >= MAX_TASKS ||
        tasks[current_task].state != TASK_RUNNING)
        return;
    if (tasks[current_task].state == TASK_RUNNING)
    {
        tasks[current_task].state = TASK_TERMINATED;
        serial_log(LOG_INFO, "CURRENT TASK TERMINATED");
        reschedule_requested = 1;
    }
}

void scheduler_block_task(u32 task_id)
{
    if (task_id >= MAX_TASKS ||
        tasks[task_id].state == TASK_UNUSED)
        return;
    if (tasks[task_id].state == TASK_READY ||
        tasks[task_id].state == TASK_RUNNING)
    {
        tasks[task_id].state = TASK_BLOCKED;
        serial_log(LOG_INFO, "TASK BLOCKED");
        reschedule_requested = 1;
    }
}

void scheduler_wake_task(u32 task_id)
{
    if (task_id >= MAX_TASKS ||
        tasks[task_id].state == TASK_UNUSED)
        return;
    if (tasks[task_id].state == TASK_BLOCKED)
    {
        tasks[task_id].state = TASK_READY;
        tasks[task_id].time_slice = get_time_slice(tasks[task_id].queue_level);
        tasks[task_id].ticks_used = 0;
        serial_log(LOG_INFO, "TASK WOKEN");
        reschedule_requested = 1;
    }
}

/*
 * Wake an interactive task and promote it by one MLFQ level.
 * Mutex waiters must continue using scheduler_wake_task().
 */
void scheduler_wake_interactive_task(u32 task_id)
{
    if (task_id >= MAX_TASKS ||
        tasks[task_id].state == TASK_UNUSED)
        return;

    if (tasks[task_id].state != TASK_BLOCKED)
        return;

    if (tasks[task_id].queue_level > 0)
    {
        tasks[task_id].queue_level--;
        serial_log(LOG_INFO, "MLFQ INTERACTIVE PROMOTION");
    }

    tasks[task_id].state = TASK_READY;
    tasks[task_id].time_slice =
        get_time_slice(tasks[task_id].queue_level);
    tasks[task_id].ticks_used = 0;

    serial_log(LOG_INFO, "INTERACTIVE TASK WOKEN");
    reschedule_requested = 1;
}
void scheduler_request_reschedule(void)
{
    reschedule_requested = 1;
}

void scheduler_yield(void)
{
    reschedule_requested = 1;
}

void scheduler_boost_task(u32 task_id, u32 priority)
{
    if (task_id >= MAX_TASKS ||
        tasks[task_id].state == TASK_UNUSED)
        return;

    if (priority >= MLFQ_LEVELS)
        priority = MLFQ_LEVELS - 1;

    if (priority < tasks[task_id].effective_priority)
    {
        tasks[task_id].effective_priority = priority;
        tasks[task_id].priority = priority;

        serial_log(LOG_INFO, "PRIORITY INHERIT");
        reschedule_requested = 1;
    }
}

void scheduler_restore_priority(u32 task_id)
{
    if (task_id >= MAX_TASKS ||
        tasks[task_id].state == TASK_UNUSED)
        return;

    tasks[task_id].effective_priority =
        tasks[task_id].base_priority;

    tasks[task_id].priority =
        tasks[task_id].base_priority;

    serial_log(LOG_INFO, "PRIORITY RESTORED");
    reschedule_requested = 1;
}

u32 scheduler_get_task_priority(u32 task_id)
{
    if (task_id >= MAX_TASKS ||
        tasks[task_id].state == TASK_UNUSED)
        return MLFQ_LEVELS - 1;
    return tasks[task_id].effective_priority;
}

u32 *scheduler_tick(u32 *current_context)
{
    __asm__ volatile ("outb %b0, $0xE9" : : "a"('S'));
    u32 tick = timer_get_ticks();

    if (tick != 0 && (tick % 10) == 0)
    {
        serial_log_performance_sample(
            tick, scheduler_switches, current_task,
            pmm_get_used_pages(), pmm_get_total_pages()
        );
    }

    if (current_is_idle)
        reap_terminated_tasks(-1);
    else
        reap_terminated_tasks((int)current_task);

    if (task_count == 0)
        return current_context;

    /*
     * First scheduler entry.
     */
    if (!scheduler_started)
    {
        __asm__ volatile ("outb %b0, $0xE9" : : "a"('!'));
        int first = find_next_task();
        __asm__ volatile ("outb %b0, $0xE9" : : "a"('@'));

        if (first >= 0)
        {
            current_task = (u32)first;
            tasks[current_task].state = TASK_RUNNING;
            tasks[current_task].ticks_used = 0;

            set_kernel_stack(
                (u32)tasks[current_task].stack + TASK_STACK_SIZE
            );
            __asm__ volatile ("outb %b0, $0xE9" : : "a"('#'));

            scheduler_started = 1;
            current_is_idle = 0;
            reschedule_requested = 0;

            serial_log(LOG_INFO, "SCHEDULER STARTED");
            __asm__ volatile ("outb %b0, $0xE9" : : "a"('$'));
            return tasks[current_task].context;
        }

        /*
         * No normal task is ready. Start the idle task.
         */
        idle_context[12] = 32;
        set_kernel_stack((u32)idle_stack + TASK_STACK_SIZE);

        scheduler_started = 1;
        current_is_idle = 1;
        reschedule_requested = 0;

        serial_log(LOG_INFO, "SCHEDULER STARTED IDLE");
        return idle_context;
    }

    /*
     * Idle task is currently running.
     * Save its interrupt context and check for newly ready tasks.
     */
    if (current_is_idle)
    {
        idle_context = current_context;

        int next = find_next_task();

        if (next < 0)
            return current_context;

        current_task = (u32)next;
        current_is_idle = 0;

        tasks[current_task].state = TASK_RUNNING;
        tasks[current_task].ticks_used = 0;

        set_kernel_stack(
            (u32)tasks[current_task].stack + TASK_STACK_SIZE
        );

        scheduler_switches++;
        reschedule_requested = 0;

        serial_log(LOG_INFO, "IDLE TO TASK SWITCH");
        return tasks[current_task].context;
    }

    /*
     * Save the currently running normal task context.
     */
    tasks[current_task].context = current_context;

    if (tasks[current_task].state == TASK_READY)
        tasks[current_task].state = TASK_RUNNING;

    if (tasks[current_task].state == TASK_RUNNING)
        tasks[current_task].ticks_used++;
    /*
     * Periodic MLFQ aging to prevent starvation.
     */
    if (tick != 0 && (tick % 100) == 0)
    {
        for (u32 i = 0; i < MAX_TASKS; i++)
        {
            if (tasks[i].state == TASK_READY &&
                tasks[i].queue_level > 0)
            {
                tasks[i].queue_level--;
                tasks[i].time_slice =
                    get_time_slice(tasks[i].queue_level);
                tasks[i].ticks_used = 0;
            }
        }

        serial_log(LOG_INFO, "MLFQ AGING BOOST");
        reschedule_requested = 1;
    }
    /*
     * MLFQ time-slice handling.
     */
    if (tasks[current_task].state == TASK_RUNNING &&
        tasks[current_task].ticks_used >= tasks[current_task].time_slice)
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

    /*
     * Switch when the current task blocks, terminates,
     * or requests rescheduling.
     */
    if (tasks[current_task].state == TASK_BLOCKED ||
        tasks[current_task].state == TASK_TERMINATED ||
        reschedule_requested)
    {
        u32 previous_task = current_task;
        int current_was_running =
            tasks[previous_task].state == TASK_RUNNING;

        if (current_was_running)
            tasks[previous_task].state = TASK_READY;

        int next = find_next_task();

        if (next >= 0)
        {
            if ((u32)next == previous_task)
            {
                tasks[previous_task].state = TASK_RUNNING;
                reschedule_requested = 0;
                return current_context;
            }

            current_task = (u32)next;
            tasks[current_task].state = TASK_RUNNING;
            tasks[current_task].ticks_used = 0;

            set_kernel_stack(
                (u32)tasks[current_task].stack + TASK_STACK_SIZE
            );
            __asm__ volatile ("outb %b0, $0xE9" : : "a"('#'));

            scheduler_switches++;
            reschedule_requested = 0;
            serial_log(LOG_INFO, "TASK SWITCH");
            return tasks[current_task].context;
        }

        if (current_was_running)
        {
            tasks[previous_task].state = TASK_RUNNING;
            reschedule_requested = 0;
            return current_context;
        }

        current_is_idle = 1;
        set_kernel_stack((u32)idle_stack + TASK_STACK_SIZE);
        scheduler_switches++;
        reschedule_requested = 0;
        serial_log(LOG_INFO, "TASK TO IDLE SWITCH");
        return idle_context;
    }

    return current_context;
}
