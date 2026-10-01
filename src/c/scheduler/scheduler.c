#include "scheduler.h"
#include "../drivers/serial_port/serial_port.h"
#include "../memory/pmm.h"

/*

* Maximum number of tasks.
  */
  static task_t tasks[MAX_TASKS];

/*

* Number of tasks currently created.
  */
  static u32 task_count = 0;

/*

* Index of currently running task.
  */
  static u32 current_task = 0;
static int scheduler_started = 0;
/*

* Scheduler reschedule request.
  */
  static volatile u32 reschedule_requested = 0;

/*

* Time slices for MLFQ queues.
*
* Queue 0 = highest priority
* Queue 1 = medium
* Queue 2 = lowest
  */
  static const u32 queue_time_slices[MLFQ_LEVELS] =
  {
  1,
  2,
  4
  };

/*

* Return the time slice belonging to
* a queue level.
  */
  static u32 get_time_slice(u32 queue_level)
  {
  if (queue_level >= MLFQ_LEVELS)
  {
  queue_level = MLFQ_LEVELS - 1;
  }

  return queue_time_slices[queue_level];
  }

/*

* Find the next READY task.
*
* Lower queue number means higher priority.
*
* The search starts after the current task so
* tasks at the same queue get a basic
* round-robin behavior.
  */
  static int find_next_task(void)
  {
  if (task_count == 0)
  {
  return -1;
  }

  int best_task = -1;
  u32 best_queue = MLFQ_LEVELS;

  /*

  * First search after current_task.
    */
    for (u32 offset = 1; offset <= task_count; offset++)
    {
    u32 index =
    (current_task + offset) % task_count;

    if (tasks[index].state == TASK_READY)
    {
    if (tasks[index].queue_level < best_queue)
    {
    best_queue =
    tasks[index].queue_level;

    
         best_task =
             (int)index;
     }
    

    }
    }

  /*

  * If no task was found after current_task,
  * search the entire task list.
    */
    if (best_task == -1)
    {
    for (u32 i = 0; i < task_count; i++)
    {
    if (tasks[i].state == TASK_READY)
    {
    if (tasks[i].queue_level < best_queue)
    {
    best_queue =
    tasks[i].queue_level;

    
             best_task =
                 (int)i;
         }
     }
    

    }
    }

  return best_task;
  }

/*

* Initialize scheduler.
  */
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

   tasks[i].time_slice =
       get_time_slice(2);

   tasks[i].ticks_used = 0;

   tasks[i].entry = 0;
   tasks[i].stack = 0;
   tasks[i].context = 0;
  

  }

  serial_log(
  LOG_INFO,
  "SCHEDULER INITIALIZED"
  );
  }

/*

* Create a new task.
*
* A task receives:
*
* * its own 4 KiB stack
* * an initial interrupt context
* * a priority
* * an MLFQ queue
    */
    int task_create(
    task_entry_t entry,
    u32 priority
    )
    {
    if (task_count >= MAX_TASKS)
    {
    serial_log(
    LOG_ERROR,
    "TASK CREATE FAILED: MAX TASKS"
    );

    return -1;
    }

  if (entry == 0)
  {
  serial_log(
  LOG_ERROR,
  "TASK CREATE FAILED: NULL ENTRY"
  );

  
   return -1;
  

  }

  /*

  * Limit priority to our three queues.
    */
    if (priority >= MLFQ_LEVELS)
    {
    priority =
    MLFQ_LEVELS - 1;
    }

  u32 id = task_count;

  task_t *task =
  &tasks[id];

  /*

  * Allocate task stack.
    */
    void *stack =
    pmm_alloc_page();

  if (stack == 0)
  {
  serial_log(
  LOG_ERROR,
  "TASK CREATE FAILED: NO STACK"
  );

  
   return -1;
  

  }

  task->id = id;

  task->state =
  TASK_READY;

  task->priority =
  priority;

  task->base_priority =
  priority;

  task->effective_priority =
  priority;

  task->queue_level =
  priority;

  task->base_queue_level =
  priority;

  task->time_slice =
  get_time_slice(
  task->queue_level
  );

  task->ticks_used = 0;

  task->entry =
  entry;

  task->stack =
  stack;

  /*

  * The interrupt assembly restores:
  *
  * GS
  * FS
  * ES
  * DS
  * EDI
  * ESI
  * EBP
  * ESP
  * EBX
  * EDX
  * ECX
  * EAX
  * INTERRUPT
  * EIP
  * CS
  * EFLAGS
  *
  * Build exactly that layout.
    */
    u32 *context =
    (u32 *)(
    (u32)stack +
    TASK_STACK_SIZE -
    (16 * sizeof(u32))
    );

  /* Segment registers. */
context[0] = 0x10; /* GS */
context[1] = 0x10; /* FS */
context[2] = 0x10; /* ES */
context[3] = 0x10; /* DS */

/* PUSHA register layout as seen from the saved stack. */
context[4] = 0; /* EDI */
context[5] = 0; /* ESI */
context[6] = 0; /* EBP */
context[7] = 0; /* ESP - ignored by POPA */
context[8] = 0; /* EBX */
context[9] = 0; /* EDX */
context[10] = 0; /* ECX */
context[11] = 0; /* EAX */

/* Interrupt return frame. */
context[12] = 32;          /* IRQ0 */
context[13] = (u32)entry;  /* EIP */
context[14] = 0x08;        /* CS */
context[15] = 0x202;       /* EFLAGS */

  task->context =
  context;

  task_count++;

  serial_log(
  LOG_INFO,
  "TASK CREATED"
  );

  return (int)id;
  }

/*

* Return current task ID.
  */
  u32 scheduler_get_current_task_id(void)
  {
  return current_task;
  }

/*

* Block the currently running task.
  */
  void scheduler_block_current(void)
  {
  if (task_count == 0)
  {
  return;
  }

  if (
  tasks[current_task].state ==
  TASK_RUNNING
  )
  {
  tasks[current_task].state =
  TASK_BLOCKED;

  
   serial_log(
       LOG_INFO,
       "CURRENT TASK BLOCKED"
   );

   reschedule_requested = 1;
  

  }
  }

/*

* Block a specific task.
*
* This is used by the priority-inversion
* demonstration before interrupts are enabled.
  */
  void scheduler_block_task(u32 task_id)
  {
  if (task_id >= task_count)
  {
  return;
  }

  if (
  tasks[task_id].state ==
  TASK_READY
  ||
  tasks[task_id].state ==
  TASK_RUNNING
  )
  {
  tasks[task_id].state =
  TASK_BLOCKED;

  
   serial_log(
       LOG_INFO,
       "TASK BLOCKED"
   );

   reschedule_requested = 1;
  

  }
  }

/*

* Wake a blocked task.
  */
  void scheduler_wake_task(u32 task_id)
  {
  if (task_id >= task_count)
  {
  return;
  }

  if (
  tasks[task_id].state ==
  TASK_BLOCKED
  )
  {
  tasks[task_id].state =
  TASK_READY;

  
   /*
    * Start the task from its current
    * effective priority.
    */
   tasks[task_id].time_slice =
       get_time_slice(
           tasks[task_id].queue_level
       );

   tasks[task_id].ticks_used = 0;

   serial_log(
       LOG_INFO,
       "TASK WOKEN"
   );

   reschedule_requested = 1;
  

  }
  }

/*

* Request rescheduling on the next
* timer interrupt.
  */
  void scheduler_request_reschedule(void)
  {
  reschedule_requested = 1;
  }

/*

* Voluntarily yield the CPU.
*
* The actual context switch occurs on the
* next timer interrupt.
  */
  void scheduler_yield(void)
  {
  reschedule_requested = 1;
  }

/*

* Priority inheritance.
*
* Lower numeric priority means higher priority.
*
* Example:
*
* LOW = 2
* HIGH = 0
*
* HIGH waits for LOW.
*
* LOW becomes effective priority 0.
  */
  void scheduler_boost_task(
  u32 task_id,
  u32 priority
  )
  {
  if (task_id >= task_count)
  {
  return;
  }

  if (priority >= MLFQ_LEVELS)
  {
  priority =
  MLFQ_LEVELS - 1;
  }

  /*

  * Only boost if the new priority
  * is actually higher.
    */
    if (
    priority <
    tasks[task_id].effective_priority
    )
    {
    tasks[task_id].effective_priority =
    priority;

    tasks[task_id].priority =
    priority;

    tasks[task_id].queue_level =
    priority;

    tasks[task_id].time_slice =
    get_time_slice(priority);

    tasks[task_id].ticks_used = 0;

    serial_log(
    LOG_INFO,
    "PRIORITY INHERIT"
    );

    reschedule_requested = 1;
    }
    }

/*

* Restore the task's original priority
* after the mutex is released.
  */
  void scheduler_restore_priority(
  u32 task_id
  )
  {
  if (task_id >= task_count)
  {
  return;
  }

  tasks[task_id].effective_priority =
  tasks[task_id].base_priority;

  tasks[task_id].priority =
  tasks[task_id].base_priority;

  tasks[task_id].queue_level =
  tasks[task_id].base_queue_level;

  tasks[task_id].time_slice =
  get_time_slice(
  tasks[task_id].queue_level
  );

  tasks[task_id].ticks_used = 0;

  serial_log(
  LOG_INFO,
  "PRIORITY RESTORED"
  );

  reschedule_requested = 1;
  }

/*

* Return the task's effective priority.
  */
  u32 scheduler_get_task_priority(
  u32 task_id
  )
  {
  if (task_id >= task_count)
  {
  return MLFQ_LEVELS - 1;
  }

  return tasks[task_id].effective_priority;
  }

/*

* Scheduler tick.
*
* Called by IRQ0.
*
* current_context points to the context
* of the task that was interrupted.
  */
  u32 *scheduler_tick(
  u32 *current_context
  )
  {
  if (task_count == 0)
  {
  return current_context;
  }
/*
 * The first timer interrupt occurs while kernel_entry()
 * is still running. It is not task 0.
 *
 * Start the first READY task using its synthetic
 * interrupt context.
 */
if (!scheduler_started)
{
    int first = find_next_task();

    if (first >= 0)
    {
        current_task = (u32)first;

        tasks[current_task].state = TASK_RUNNING;
        tasks[current_task].ticks_used = 0;

        scheduler_started = 1;
        reschedule_requested = 0;

        serial_log(
            LOG_INFO,
            "SCHEDULER STARTED"
        );

        return tasks[current_task].context;
    }

    return current_context;
}

  /*

  * Save the context of the currently
  * executing task.
    */
    tasks[current_task].context =
    current_context;

  /*

  * Mark current task RUNNING if it
  * was READY.
    */
    if (
    tasks[current_task].state ==
    TASK_READY
    )
    {
    tasks[current_task].state =
    TASK_RUNNING;
    }

  /*

  * Account for one timer tick.
    */
    tasks[current_task].ticks_used++;

  /*

  * MLFQ time slice expired.
    */
    if (
    tasks[current_task].ticks_used >=
    tasks[current_task].time_slice
    )
    {
    /*

    * CPU-bound task gets demoted.
      */
      if (
      tasks[current_task].queue_level <
      MLFQ_LEVELS - 1
      )
      {
      tasks[current_task].queue_level++;

      serial_log(
      LOG_INFO,
      "MLFQ QUEUE DEMOTION"
      );
      }

    tasks[current_task].time_slice =
    get_time_slice(
    tasks[current_task].queue_level
    );

    tasks[current_task].ticks_used =
    0;

    reschedule_requested =
    1;
    }

  /*

  * Decide whether we need another task.
    */
    if (
    tasks[current_task].state ==
    TASK_BLOCKED
    ||
    tasks[current_task].state ==
    TASK_TERMINATED
    ||
    reschedule_requested
    )
    {
    int next =
    find_next_task();

    if (next >= 0)
    {
    /*
    * Current task becomes READY again
    * if it is still runnable.
    */
    if (
    tasks[current_task].state ==
    TASK_RUNNING
    )
    {
    tasks[current_task].state =
    TASK_READY;
    }

    
     current_task =
         (u32)next;

     tasks[current_task].state =
         TASK_RUNNING;

     tasks[current_task].ticks_used =
         0;

     reschedule_requested =
         0;

     serial_log(
         LOG_INFO,
         "TASK SWITCH"
     );

     return tasks[current_task].context;
    

    }
    }

  return current_context;
  }

