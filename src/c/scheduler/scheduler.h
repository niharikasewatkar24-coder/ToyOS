#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "../kernel/kernel.h"

#define MAX_TASKS 8
#define MLFQ_LEVELS 3
#define TASK_STACK_SIZE 4096

typedef enum
{
    TASK_UNUSED,
    TASK_READY,
    TASK_RUNNING,
    TASK_BLOCKED,
    TASK_TERMINATED
} task_state_t;

typedef void (*task_entry_t)(void);

typedef struct
{
    u32 id;
    task_state_t state;

    u32 priority;

/* Original priority before priority inheritance. */
u32 base_priority;

/* Effective priority after priority inheritance. */
u32 effective_priority;

u32 queue_level;
u32 base_queue_level;

    u32 time_slice;
    u32 ticks_used;

    task_entry_t entry;
    void *stack;

    u32 *context;
} task_t;

void scheduler_init(void);

/*
 * Called by the timer interrupt.
 *
 * current_context points to the register/iret frame
 * created by the interrupt assembly.
 *
 * Returns the context that the assembly should restore.
 */
u32 *scheduler_tick(u32 *current_context);

int task_create(task_entry_t entry, u32 priority);


/*
 * Synchronization support.
 */

/* Get the ID of the currently running task. */
u32 scheduler_get_current_task_id(void);

/* Block the currently running task. */
void scheduler_block_current(void);

/* Wake a blocked task and make it READY. */
void scheduler_wake_task(u32 task_id);
/*
 * Voluntarily give up the CPU.
 */
void scheduler_yield(void);
void scheduler_request_reschedule(void);
void scheduler_block_task(u32 task_id);

void scheduler_boost_task(
    u32 task_id,
    u32 priority
);

void scheduler_restore_priority(
    u32 task_id
);

u32 scheduler_get_task_priority(
    u32 task_id
);
#endif
