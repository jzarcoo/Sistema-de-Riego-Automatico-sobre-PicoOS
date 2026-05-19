#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdint.h>
#include "pico/multicore.h"

#define MAX_TASKS_PER_CORE 5
#define STACK_SIZE 512

#define TASK_TIMEOUT_TICKS 500

typedef enum {
    DORMANT,
    READY,
    RUNNING,
    BLOCKED
} task_state_t;

typedef struct {
    uint32_t *sp;
    task_state_t state;
    uint32_t stack[STACK_SIZE];
    int quantum;
    int remaining_ticks;
    void (*entry_point)(void);
    uint32_t heartbeat;
    uint32_t last_seen;
    uint32_t wake_tick;
} tcb_t;

typedef struct {
    tcb_t tasks[MAX_TASKS_PER_CORE];
    int current_task;
    volatile uint32_t kernel_ticks;
    int num_tasks;
} core_scheduler_t;

extern core_scheduler_t core_schedulers[2];

void task_create_on_core(int core_id, int id, void (*entry_point)(void));
void k_task_exit(void);
void k_task_sleep(uint32_t ms);
uint32_t schedule(uint32_t current_sp);

#endif