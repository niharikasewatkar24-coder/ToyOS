
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
    u32 base_priority;
    u32 effective_priority;
    u32 queue_level;
    u32 base_queue_level;
    u32 time_slice;
    u32 ticks_used;

    task_entry_t entry;

    void *stack;
    void *user_stack;
    u32 *context;

} task_t;

/* Initialize scheduler. */
void scheduler_init(void);

/* Timer interrupt scheduler. */
u32 *scheduler_tick(u32 *current_context);

/* Create tasks. */
int task_create(task_entry_t entry, u32 priority);

int task_create_user(
    u32 entry_address,
    u32 user_stack_top,
    u32 priority
);

/* Current task information. */
u32 scheduler_get_current_task_id(void);

/* Task blocking and waking. */
void scheduler_block_current(void);
void scheduler_block_task(u32 task_id);
void scheduler_wake_task(u32 task_id);

/* Task termination. */
void scheduler_terminate_current(void);

/* CPU scheduling controls. */
void scheduler_yield(void);
void scheduler_request_reschedule(void);

/* Priority inheritance support. */
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
