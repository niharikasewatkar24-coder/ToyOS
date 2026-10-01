#include "mutex.h"

#include "../drivers/serial_port/serial_port.h"


/*
 * Initialize mutex.
 */
void mutex_init(
    mutex_t *mutex
)
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
void mutex_lock(
    mutex_t *mutex
)
{
    u32 current =
        scheduler_get_current_task_id();


    while (1)
    {
        /*
         * Mutex is free.
         */
        if (!mutex->locked)
        {
            mutex->locked = 1;

            mutex->owner_task =
                current;


            serial_log(
                LOG_INFO,
                "LOCK ACQUIRED"
            );


            return;
        }


        /*
         * Current task already owns it.
         */
        if (
            mutex->owner_task ==
            current
        )
        {
            serial_log(
                LOG_INFO,
                "LOCK ALREADY OWNED"
            );


            return;
        }


        /*
         * Add task to waiter queue.
         *
         * For this project demonstration there is
         * only one expected waiter, but we keep a
         * proper queue structure.
         */
        int already_waiting = 0;


        for (
            u32 i = 0;
            i < mutex->waiter_count;
            i++
        )
        {
            if (
                mutex->waiters[i] ==
                current
            )
            {
                already_waiting = 1;

                break;
            }
        }


        if (
            !already_waiting &&
            mutex->waiter_count <
                MAX_TASKS
        )
        {
            mutex->waiters[
                mutex->waiter_count
            ] = current;


            mutex->waiter_count++;
        }


        /*
         * Priority inheritance.
         *
         * The waiting task donates its effective
         * priority to the mutex owner.
         */
        u32 waiting_priority =
            scheduler_get_task_priority(
                current
            );


        scheduler_boost_task(
            mutex->owner_task,
            waiting_priority
        );


        serial_log(
            LOG_INFO,
            "LOCK BLOCKED"
        );


        /*
         * Block the waiting task.
         */
        scheduler_block_current();


        /*
         * Tell scheduler to switch away from
         * this task at the next timer interrupt.
         */
        scheduler_request_reschedule();


        /*
         * When this task wakes up, execution
         * returns here and the mutex is checked
         * again.
         */
    }
}


/*
 * Release mutex.
 */
void mutex_unlock(
    mutex_t *mutex
)
{
    u32 current =
        scheduler_get_current_task_id();


    /*
     * Only owner can unlock.
     */
    if (
        !mutex->locked ||
        mutex->owner_task != current
    )
    {
        serial_log(
            LOG_ERROR,
            "INVALID MUTEX UNLOCK"
        );


        return;
    }


    /*
     * Save owner.
     */
    u32 owner =
        mutex->owner_task;


    /*
     * Release mutex.
     */
    mutex->locked = 0;

    mutex->owner_task = 0;


    serial_log(
        LOG_INFO,
        "LOCK RELEASED"
    );


    /*
     * Restore original priority.
     */
    scheduler_restore_priority(
        owner
    );


    /*
     * Wake first waiter.
     */
    if (
        mutex->waiter_count > 0
    )
    {
        u32 next =
            mutex->waiters[0];


        /*
         * Shift remaining waiters.
         */
        for (
            u32 i = 1;
            i < mutex->waiter_count;
            i++
        )
        {
            mutex->waiters[i - 1] =
                mutex->waiters[i];
        }


        mutex->waiter_count--;


        /*
         * Wake waiting task.
         */
        scheduler_wake_task(
            next
        );


        serial_log(
            LOG_INFO,
            "MUTEX WAKE"
        );
    }
}
