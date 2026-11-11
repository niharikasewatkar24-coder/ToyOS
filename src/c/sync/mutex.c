
#include "mutex.h"
#include "../drivers/serial_port/serial_port.h"

/*
 * Set to 1: Priority inheritance ENABLED
 * Set to 0: Priority inheritance DISABLED
 */
#define ENABLE_PRIORITY_INHERITANCE 1


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
int mutex_lock(mutex_t *mutex)
{
    while (1)
    {
        u32 flags = irq_save();
        u32 current = scheduler_get_current_task_id();

        if (!mutex->locked)
        {
            mutex->locked = 1;
            mutex->owner_task = current;
            irq_restore(flags);
            serial_log(LOG_INFO, "LOCK ACQUIRED");
            return 1;
        }

        if (mutex->owner_task == current)
        {
            irq_restore(flags);
            serial_log(LOG_INFO, "LOCK ALREADY OWNED");
            return 1;
        }

        int already_waiting = 0;

        for (u32 i = 0; i < mutex->waiter_count; i++)
        {
            if (mutex->waiters[i] == current)
            {
                already_waiting = 1;
                break;
            }
        }

        if (!already_waiting)
        {
            if (mutex->waiter_count >= MAX_TASKS)
            {
                irq_restore(flags);
                serial_log(LOG_ERROR, "MUTEX WAIT QUEUE FULL");
                return 0;
            }

            mutex->waiters[mutex->waiter_count++] = current;
        }

#if ENABLE_PRIORITY_INHERITANCE
        u32 waiting_priority =
            scheduler_get_task_priority(current);

        scheduler_boost_task(
            mutex->owner_task,
            waiting_priority
        );
#endif

        serial_log(LOG_INFO, "LOCK BLOCKED");

        scheduler_block_current();
        scheduler_request_reschedule();

        irq_restore(flags);
    }
}


/*
 * Release mutex.
 */
void mutex_unlock(mutex_t *mutex)
{
    u32 flags = irq_save();
    u32 current = scheduler_get_current_task_id();

    if (!mutex->locked || mutex->owner_task != current)
    {
        irq_restore(flags);
        serial_log(LOG_ERROR, "INVALID MUTEX UNLOCK");
        return;
    }

#if ENABLE_PRIORITY_INHERITANCE
    scheduler_restore_priority(current);
#endif

    if (mutex->waiter_count > 0)
    {
        u32 next = mutex->waiters[0];

        for (u32 i = 1; i < mutex->waiter_count; i++)
        {
            mutex->waiters[i - 1] = mutex->waiters[i];
        }

        mutex->waiter_count--;
        mutex->owner_task = next;

#if ENABLE_PRIORITY_INHERITANCE
        for (u32 i = 0; i < mutex->waiter_count; i++)
        {
            u32 waiting_priority =
                scheduler_get_task_priority(mutex->waiters[i]);

            scheduler_boost_task(next, waiting_priority);
        }
#endif

        scheduler_wake_task(next);

        irq_restore(flags);

        serial_log(LOG_INFO, "LOCK RELEASED");
        serial_log(LOG_INFO, "MUTEX WAKE");
        return;
    }

    mutex->locked = 0;
    mutex->owner_task = 0;

    irq_restore(flags);

    serial_log(LOG_INFO, "LOCK RELEASED");
}
