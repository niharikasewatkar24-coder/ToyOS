#ifndef SCHED_H
#define SCHED_H

#include <stdint.h>

#define MAX_TASKS 16
#define STACK_SIZE 4096

typedef enum {
    TASK_UNUSED = 0,
    TASK_READY,
    TASK_RUNNING,
    TASK_BLOCKED,
    TASK_FINISHED
} task_state_t;

struct lock;

typedef struct task {
    uint32_t *esp;
    uint8_t stack[STACK_SIZE];
    uint32_t pid;
    char name[16];
    task_state_t state;
    int q;
    int base_q;
    uint32_t cpu_ticks;
    uint32_t blocked_ticks;
    uint32_t avgwait;
    uint32_t turnaround;
    uint32_t arrival_tick;
    uint32_t finish_tick;
    uint32_t ticks_in_slice;
    struct lock *waiting_on;

    /* Additional tracking fields */
    void (*entry)(void);
    uint32_t wait_ticks;
    uint32_t wait_count;
    int is_interactive;
} task_t;

typedef struct lock {
    int locked;
    task_t *holder;
} lock_t;

extern void kprintf(const char *f, ...);
extern void switch_to(uint32_t **prev_esp, uint32_t *next_esp);

void sched_init(void);
void sched_tick(void);
void sched_yield(void);

/* Task, MLFQ and Lock API */
void sched_set_pi(int enabled);
void sched_set_mlfq(int enabled);
void sched_set_tick(uint32_t tick);
void sched_set_context_switches(uint32_t cs);
void sched_set_pi_boosts(uint32_t pb);
void sched_init_test(int pi, int mlfq, uint32_t start_tick);
task_t *task_create(const char *name, void (*entry)(void), int q, int is_interactive);
void task_set_stats(const char *name, uint32_t cpu, uint32_t blocked, uint32_t avgwait, uint32_t turnaround);
void task_burn(uint32_t ticks);
void task_exit(void);
void lock_init(lock_t *lk);
void lock_acquire(lock_t *lk);
void lock_release(lock_t *lk);
void sched_run(void);
void sched_report(int pi_enabled);

#endif
