#ifndef MUTEX_H
#define MUTEX_H

#include "../kernel/kernel.h"
#include "../scheduler/scheduler.h"

typedef struct
{
    int locked;
    u32 owner_task;

    u32 waiters[MAX_TASKS];
    u32 waiter_count;
} mutex_t;

void mutex_init(mutex_t *mutex);
void mutex_lock(mutex_t *mutex);
void mutex_unlock(mutex_t *mutex);

#endif
