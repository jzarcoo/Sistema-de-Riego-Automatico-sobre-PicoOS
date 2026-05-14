/**
 * @file scheduler.h
 * @brief Definiciones del planificador Round-Robin dual-core.
 *
 * Define el TCB (Task Control Block), los estados de tarea,
 * y la estructura del scheduler por core. Cada core tiene su
 * propio arreglo de tareas, tick counter y tarea activa.
 */

#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdint.h>
#include "pico/multicore.h"

/** Maximo de tareas por core */
#define MAX_TASKS_PER_CORE 5

/** Tamano del stack de cada tarea (en words de 32 bits) */
#define STACK_SIZE 512

/** Ticks sin heartbeat antes de reiniciar una tarea */
#define TASK_TIMEOUT_TICKS 500

/** Estados posibles de una tarea */
typedef enum {
    DORMANT,    /** Tarea inactiva/terminada */
    READY,      /** Lista para ejecutar */
    RUNNING,    /** Actualmente en ejecucion */
    BLOCKED     /** Bloqueada (sleep o semaforo) */
} task_state_t;

/**
 * @brief Task Control Block — estructura de control de cada tarea.
 *
 * Contiene el stack pointer, estado, stack propio, parametros de
 * planificacion (quantum), y datos del watchdog (heartbeat/last_seen).
 */
typedef struct {
    uint32_t *sp;                   /** Stack pointer actual */
    task_state_t state;             /** Estado de la tarea */
    uint32_t stack[STACK_SIZE];     /** Stack dedicado de la tarea */
    int quantum;                    /** Quantum asignado en ticks */
    int remaining_ticks;            /** Ticks restantes del quantum actual */
    void (*entry_point)(void);      /** Funcion de entrada (loop infinito) */
    uint32_t heartbeat;             /** Contador de heartbeats reportados */
    uint32_t last_seen;             /** Ultimo tick en que reporto heartbeat */
    uint32_t wake_tick;             /** Tick en que debe despertar (si BLOCKED por sleep) */
} tcb_t;

/**
 * @brief Estructura del scheduler de un core.
 *
 * Cada core del RP2040 tiene su instancia independiente con su
 * propio arreglo de tareas y tick counter.
 */
typedef struct {
    tcb_t tasks[MAX_TASKS_PER_CORE];    /** Arreglo de TCBs */
    int current_task;                    /** Indice de la tarea activa (-1 = ninguna) */
    volatile uint32_t kernel_ticks;      /** Contador de ticks del sistema */
    int num_tasks;                       /** Cantidad de tareas registradas */
} core_scheduler_t;

/** Schedulers globales, uno por core */
extern core_scheduler_t core_schedulers[2];

/** Crea una tarea en un core especifico */
void task_create_on_core(int core_id, int id, void (*entry_point)(void));

/** Termina la tarea actual */
void k_task_exit(void);

/** Bloquea la tarea actual por ms milisegundos */
void k_task_sleep(uint32_t ms);

/** Funcion de scheduling (llamada desde PendSV) */
uint32_t schedule(uint32_t current_sp);

#endif
