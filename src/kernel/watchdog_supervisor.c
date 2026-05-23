/**
 * @file watchdog_supervisor.c
 * @brief Supervisor de heartbeat para deteccion de tareas colgadas.
 *
 * Cada tarea debe invocar kernel_task_heartbeat() periodicamente
 * (via syscall sys_heartbeat). El scheduler verifica en cada tick
 * que last_seen no exceda TASK_TIMEOUT_TICKS; si una tarea no
 * reporta heartbeat a tiempo, se reinicia automaticamente.
 * Esto permite detectar tanto tareas que acaparan CPU como tareas
 * bloqueadas indefinidamente en un semaforo (posible deadlock).
 */

#include "watchdog_supervisor.h"
#include "scheduler.h"
#include "pico/multicore.h"

/**
 * @brief Registra actividad de la tarea actual (heartbeat).
 *
 * Actualiza last_seen con el tick actual e incrementa el contador
 * de heartbeats. Debe llamarse desde el loop principal de cada tarea.
 */
void kernel_task_heartbeat(void) {
    int core_id = get_core_num();
    core_scheduler_t *sched = &core_schedulers[core_id];
    if (sched->current_task >= 0) {
        sched->tasks[sched->current_task].last_seen = sched->kernel_ticks;
        sched->tasks[sched->current_task].heartbeat++;
    }
}
