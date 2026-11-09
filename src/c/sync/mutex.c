
#include "mutex.h"
#include "../drivers/serial_port/serial_port.h"

/*
 * Set to 1: Priority inheritance ENABLED
 * Set to 0: Priority inheritance DISABLED
 */
#define ENABLE_PRIORITY_INHERITANCE 0


/*
 * Initialize mutex.
 */
void mutex_init(mutex_t *mutex)
{
    mutex->locked = 0;
    mutex->owner_task = 0;
    mutex->waiter_count = 0;

    for (u32 i = 0; i < MAX_TASKS; i++)
    {
        mutex->waiters[i] = 0;
    }
}


/*
 * Acquire mutex.
 */
void mutex_lock(mutex_t *mutex)
{
    u32 current = scheduler_get_current_task_id();

    while (1)
    {
        /* Mutex is free. */
        if (!mutex->locked)
        {
            mutex->locked = 1;
            mutex->owner_task = current;

            serial_log(LOG_INFO, "LOCK ACQUIRED");
            return;
        }

        /* Current task already owns it. */
        if (mutex->owner_task == current)
        {
            serial_log(LOG_INFO, "LOCK ALREADY OWNED");
            return;
        }

        /* Add task to waiter queue. */
        int already_waiting = 0;

        for (u32 i = 0; i < mutex->waiter_count; i++)
        {
            if (mutex->waiters[i] == current)
            {
                already_waiting = 1;
                break;
            }
        }

        if (!already_waiting &&
            mutex->waiter_count < MAX_TASKS)
        {
            mutex->waiters[mutex->waiter_count] = current;
            mutex->waiter_count++;
        }

        /*
         * Priority inheritance.
         * Enabled only when the switch is 1.
         */
#if ENABLE_PRIORITY_INHERITANCE

        u32 waiting_priority =
            scheduler_get_task_priority(current);

        scheduler_boost_task(
            mutex->owner_task,
            waiting_priority
        );

#endif

        serial_log(LOG_INFO, "LOCK BLOCKED");

        /* Block the waiting task. */
        scheduler_block_current();

        /* Request scheduler switch. */
        scheduler_request_reschedule();

        /*
         * When this task wakes up,
         * execution returns here.
         */
    }
}


/*
 * Release mutex.
 */
void mutex_unlock(mutex_t *mutex)
{
    u32 flags = irq_save();
    u32 current = scheduler_get_current_task_id();

    /* Only owner can unlock. */
    if (!mutex->locked || mutex->owner_task != current)
    {
        irq_restore(flags);
        serial_log(LOG_ERROR, "INVALID MUTEX UNLOCK");
        return;
    }

    u32 owner = mutex->owner_task;

#if ENABLE_PRIORITY_INHERITANCE
    scheduler_restore_priority(owner);
#endif

    /* Transfer ownership directly to the first waiter. */
    if (mutex->waiter_count > 0)
    {
        u32 next = mutex->waiters[0];

        for (u32 i = 1; i < mutex->waiter_count; i++)
        {
            mutex->waiters[i - 1] = mutex->waiters[i];
        }

        mutex->waiter_count--;
        mutex->owner_task = next;

        scheduler_wake_task(next);

        irq_restore(flags);

        serial_log(LOG_INFO, "LOCK RELEASED");
        serial_log(LOG_INFO, "MUTEX WAKE");
        return;
    }

    /* No waiters: fully release the mutex. */
    mutex->locked = 0;
    mutex->owner_task = 0;

    irq_restore(flags);

    serial_log(LOG_INFO, "LOCK RELEASED");
}
