#include "watchdog_supervisor.h"
#include "scheduler.h"

extern volatile uint32_t kernel_ticks;
extern int current_task;
extern tcb_t tasks[MAX_TASKS];

void kernel_task_heartbeat(void) {
    if (current_task >= 0) {
        tasks[current_task].last_seen = kernel_ticks;
        tasks[current_task].heartbeat++;
    }
}