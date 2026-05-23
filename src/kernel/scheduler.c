/**
 * @file scheduler.c
 * @brief Planificador Round-Robin cooperativo/preemptivo por core.
 *
 * Implementa un scheduler independiente para cada core del RP2040.
 * Cada core mantiene su propia tabla de tareas (TCB), tick counter,
 * y logica de conmutacion de contexto via PendSV. El SysTick
 * decrementa el quantum de la tarea activa y dispara PendSV cuando
 * se agota. Incluye deteccion de tareas colgadas (watchdog por
 * heartbeat) con reinicio automatico de la tarea afectada.
 */

#include "scheduler.h"
#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"

/** Arreglo global de schedulers, uno por core */
core_scheduler_t core_schedulers[2];

/**
 * @brief Inicializa el stack de una tarea con el frame de excepcion ARM.
 *
 * Prepara el stack para que PendSV pueda restaurar el contexto como
 * si la tarea hubiera sido interrumpida. El frame incluye xPSR con
 * bit Thumb, PC apuntando al entry_point, y LR con EXC_RETURN para PSP.
 *
 * @param core_id Core al que pertenece la tarea.
 * @param id Indice de la tarea en el arreglo del scheduler.
 */
static void init_task_stack(int core_id, int id) {
    tcb_t *t = &core_schedulers[core_id].tasks[id];
    uint32_t *stack_top = t->stack + STACK_SIZE;

    *(--stack_top) = 0x01000000;
    *(--stack_top) = (uint32_t)t->entry_point;
    *(--stack_top) = 0xFFFFFFFD;
    *(--stack_top) = 0; // R12
    *(--stack_top) = 0; // R3
    *(--stack_top) = 0; // R2
    *(--stack_top) = 0; // R1
    *(--stack_top) = 0; // R0

    for (int i = 0; i < 8; i++) {
        *(--stack_top) = 0;
    }

    t->sp = stack_top;
    t->state = READY;
}

/**
 * @brief Crea una tarea en un core especifico.
 *
 * Registra el entry_point en el TCB, inicializa quantum, heartbeat,
 * y prepara el stack. Si el id supera num_tasks, actualiza el contador.
 *
 * @param core_id Core destino (0 o 1).
 * @param id Indice del slot de tarea (0 a MAX_TASKS_PER_CORE-1).
 * @param entry_point Funcion de entrada de la tarea (loop infinito).
 */
void task_create_on_core(int core_id, int id, void (*entry_point)(void)) {
    if (id >= 0 && id < MAX_TASKS_PER_CORE) {
        core_scheduler_t *sched = &core_schedulers[core_id];
        sched->tasks[id].entry_point = entry_point;
        sched->tasks[id].state = READY;
        sched->tasks[id].quantum = 10;
        sched->tasks[id].remaining_ticks = 10;
        sched->tasks[id].heartbeat = 1;
        sched->tasks[id].last_seen = sched->kernel_ticks;
        init_task_stack(core_id, id);
        if (id >= sched->num_tasks)
            sched->num_tasks = id + 1;
    }
}

/**
 * @brief ISR del SysTick — motor del planificador preemptivo.
 *
 * Ejecuta cada tick del sistema y realiza:
 * 1. Incrementa kernel_ticks del core actual.
 * 2. Despierta tareas BLOCKED con wake_tick cumplido (sleep expiro).
 * 3. Detecta tareas colgadas: si una tarea no reporta heartbeat en
 *    TASK_TIMEOUT_TICKS, la reinicia. Para tareas BLOCKED en semaforo
 *    sin wake_tick (posible deadlock), las marca DORMANT y reinicia.
 * 4. Decrementa quantum de la tarea activa; si se agota, dispara PendSV.
 */
void isr_systick() {
    int core_id = get_core_num();
    core_scheduler_t *sched = &core_schedulers[core_id];

    sched->kernel_ticks++;

    for (int i = 0; i < sched->num_tasks; i++) {
        if (sched->tasks[i].state == DORMANT)
            continue;

        if (sched->tasks[i].state == BLOCKED && sched->tasks[i].wake_tick != 0) {
            sched->tasks[i].last_seen = sched->kernel_ticks;
            if (sched->kernel_ticks >= sched->tasks[i].wake_tick) {
                sched->tasks[i].wake_tick = 0;
                sched->tasks[i].state = READY;
            }
            continue;
        }

        if ((sched->kernel_ticks - sched->tasks[i].last_seen) > TASK_TIMEOUT_TICKS) {
            if (sched->tasks[i].state == BLOCKED) {
                sched->tasks[i].state = DORMANT;
            }
            task_create_on_core(core_id, i, sched->tasks[i].entry_point);
        }
    }

    if (sched->current_task != -1) {
        if (--sched->tasks[sched->current_task].remaining_ticks > 0) {
            return;
        }
        sched->tasks[sched->current_task].remaining_ticks = sched->tasks[sched->current_task].quantum;
    }

    *(volatile uint32_t *)0xE000ED04 = (1 << 28);
}

/**
 * @brief Termina la tarea actual y solicita cambio de contexto.
 *
 * Marca la tarea como DORMANT y dispara PendSV para que el scheduler
 * seleccione la siguiente tarea lista.
 */
void k_task_exit(void) {
    int core_id = get_core_num();
    core_scheduler_t *sched = &core_schedulers[core_id];
    if (sched->current_task != -1) {
        sched->tasks[sched->current_task].state = DORMANT;
        *(volatile uint32_t *)0xE000ED04 = (1 << 28);
    }
}

/**
 * @brief Selecciona la siguiente tarea a ejecutar (Round-Robin).
 *
 * Guarda el SP de la tarea saliente, busca la siguiente tarea READY
 * en orden circular, y la marca RUNNING. Configura el registro CONTROL
 * para ejecutar en modo no privilegiado con PSP (bit 0 = nPRIV, bit 1 = PSP).
 *
 * @param current_sp Stack pointer de la tarea que fue interrumpida.
 * @return Stack pointer de la nueva tarea a restaurar.
 */
uint32_t schedule(uint32_t current_sp) {
    int core_id = get_core_num();
    core_scheduler_t *sched = &core_schedulers[core_id];

    if (sched->current_task != -1) {
        sched->tasks[sched->current_task].sp = (uint32_t *)current_sp;
        if (sched->tasks[sched->current_task].state == RUNNING) {
            sched->tasks[sched->current_task].state = READY;
        }
    }

    int next_task = sched->current_task;
    int tries = 0;

    do {
        next_task = (next_task + 1) % sched->num_tasks;
        tries++;
        if (sched->tasks[next_task].state == READY) {
            break;
        }
    } while (tries < sched->num_tasks);

    if (sched->tasks[next_task].state == READY) {
        sched->current_task = next_task;
        sched->tasks[sched->current_task].state = RUNNING;
        sched->tasks[sched->current_task].remaining_ticks = sched->tasks[sched->current_task].quantum;

        uint32_t control;

        __asm volatile (
            "mrs %0, control"
            : "=r" (control)
        );

        control |= 0x3;

        __asm volatile (
            "msr control, %0"
            :
            : "r" (control)
        );

        __asm volatile ("isb");

        return (uint32_t)sched->tasks[sched->current_task].sp;
    }

    return current_sp;
}

/**
 * @brief Bloquea la tarea actual por un tiempo determinado.
 *
 * Calcula el tick futuro en que la tarea debe despertar, la marca
 * BLOCKED, y dispara PendSV para ceder el procesador.
 *
 * @param ms Milisegundos a dormir (resolucion: 10ms por tick).
 */
void k_task_sleep(uint32_t ms) {
    int core_id = get_core_num();
    core_scheduler_t *sched = &core_schedulers[core_id];
    if (sched->current_task < 0) return;
    uint32_t ticks_to_sleep = ms / 10;
    if (ticks_to_sleep == 0) ticks_to_sleep = 1;
    sched->tasks[sched->current_task].wake_tick = sched->kernel_ticks + ticks_to_sleep;
    sched->tasks[sched->current_task].state = BLOCKED;
    *(volatile uint32_t *)0xE000ED04 = (1 << 28);
}

/**
 * @brief Handler del SysTick (vector de excepcion).
 * Redirige al isr_systick del planificador.
 */
void SysTick_Handler(void) {
    isr_systick();
}
