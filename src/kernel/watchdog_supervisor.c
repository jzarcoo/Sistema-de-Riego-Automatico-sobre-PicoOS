#include "watchdog_supervisor.h"
#include "scheduler.h"
#include "pico/multicore.h"

void kernel_task_heartbeat(void) {
    int core_id = get_core_num();
    core_scheduler_t *sched = &core_schedulers[core_id];
    if (sched->current_task >= 0) {
        sched->tasks[sched->current_task].last_seen = sched->kernel_ticks;
        sched->tasks[sched->current_task].heartbeat++;
    }
}
