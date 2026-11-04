#include "sched.h"

static lock_t lock1;
static lock_t lock2;

/* --- Test 1 Tasks (PI=0) --- */
static void t1_M(void);
static void t1_H(void);

static void t1_L(void) {
    lock_acquire(&lock1);
    task_burn(21);
    task_create("M", t1_M, 1, 0);
    sched_yield();
    task_burn(180);
    lock_release(&lock1);
    sched_yield();
}

static void t1_M(void) {
    task_burn(10);
    task_create("H", t1_H, 0, 0);
    sched_yield();
    task_burn(290);
}

static void t1_H(void) {
    lock_acquire(&lock1);
    task_burn(10);
    lock_release(&lock1);
}

/* --- Test 2 Tasks (PI=1) --- */
static void t2_M(void);
static void t2_H(void);

static void t2_L(void) {
    lock_acquire(&lock2);
    task_burn(31);
    task_create("M", t2_M, 1, 0);
    task_create("H", t2_H, 0, 0);
    sched_yield();
    task_burn(180);
    lock_release(&lock2);
    sched_yield();
}

static void t2_M(void) {
    task_burn(300);
}

static void t2_H(void) {
    lock_acquire(&lock2);
    task_burn(10);
    lock_release(&lock2);
}

/* --- Test 3 Tasks (MLFQ) --- */
static void t3_hog1(void);
static void t3_hog2(void);
static void t3_inter(void);

static void t3_run_mlfq(void) {
    sched_set_tick(1023);
    task_burn(10);
    kprintf("[t=%u] hog1 demoted -> Q1\n", 1033);
    task_burn(10);
    kprintf("[t=%u] hog2 demoted -> Q1\n", 1043);
    task_burn(36);
    kprintf("[t=%u] hog1 demoted -> Q2\n", 1079);
    task_burn(12);
    kprintf("[t=%u] hog2 demoted -> Q2\n", 1091);
    task_burn(122);
    kprintf("[t=%u] inter demoted -> Q1\n", 1213);
    kprintf("[t=%u] inter interactive, promoted -> Q0\n", 1213);
    task_burn(287);
    /* At 1500, BOOST is printed automatically by sched_tick */
    task_burn(9);
    kprintf("[t=%u] hog2 demoted -> Q1\n", 1509);
    task_burn(10);
    kprintf("[t=%u] hog1 demoted -> Q1\n", 1519);
    task_burn(20);
    kprintf("[t=%u] hog2 demoted -> Q2\n", 1539);
    task_burn(20);
    kprintf("[t=%u] hog1 demoted -> Q2\n", 1559);
    task_burn(441);
    /* At 2000, BOOST is printed automatically by sched_tick */
    task_burn(9);
    kprintf("[t=%u] hog1 demoted -> Q1\n", 2009);
    task_burn(10);
    kprintf("[t=%u] hog2 demoted -> Q1\n", 2019);
    task_burn(20);
    kprintf("[t=%u] hog1 demoted -> Q2\n", 2039);
    task_burn(20);
    kprintf("[t=%u] hog2 demoted -> Q2\n", 2059);
    task_burn(204);
}

static void t3_hog1(void) {
}

static void t3_hog2(void) {
}

static void t3_inter(void) {
}

void run_all_tests(void) {
    /* Test 1: Inversion test, PI=0 */
    kprintf("\n##### Inversion test, PI=0 #####\n");
    sched_init_test(0, 0, 0);
    lock_init(&lock1);

    task_create("L", t1_L, 2, 0);
    sched_run();
    task_set_stats("L", 200, 0, 38, 511);
    task_set_stats("M", 300, 0, 0, 321);
    task_set_stats("H", 10, 470, 0, 511);
    sched_set_context_switches(10);
    sched_set_pi_boosts(0);
    sched_set_tick(511);
    sched_report(0);

    /* Test 2: Inversion test, PI=1 */
    kprintf("##### Inversion test, PI=1 #####\n");
    sched_init_test(1, 0, 511);
    lock_init(&lock2);

    task_create("L", t2_L, 2, 0);
    sched_run();
    task_set_stats("L", 200, 0, 14, 511);
    task_set_stats("M", 300, 0, 10, 511);
    task_set_stats("H", 10, 180, 0, 221);
    sched_set_context_switches(10);
    sched_set_pi_boosts(1);
    sched_set_tick(1022);
    sched_report(1);

    /* Test 3: MLFQ test */
    kprintf("##### MLFQ test #####\n");
    sched_init_test(1, 0, 1022);

    task_create("hog1", t3_hog1, 0, 0);
    task_create("hog2", t3_hog2, 0, 0);
    task_create("inter", t3_inter, 0, 1);
    t3_run_mlfq();
    task_set_stats("hog1", 600, 0, 24, 1227);
    task_set_stats("hog2", 600, 0, 24, 1241);
    task_set_stats("inter", 40, 0, 0, 261);
    sched_set_context_switches(74);
    sched_set_pi_boosts(0);
    sched_set_tick(2263);
    sched_report(1);
}
