/**
 * @file watchdog_supervisor.h
 * @brief Interfaz del supervisor de heartbeat para deteccion de tareas colgadas.
 *
 * Las tareas deben llamar a sys_heartbeat() periodicamente.
 * El kernel verifica en isr_systick() que last_seen no exceda el timeout.
 */

#ifndef WATCHDOG_SUPERVISOR_H
#define WATCHDOG_SUPERVISOR_H

/** Registra heartbeat de la tarea actual (actualiza last_seen) */
void kernel_task_heartbeat(void);

#endif
