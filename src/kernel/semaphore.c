/**
 * @file semaphore.c
 * @brief Semaforos de conteo para sincronizacion entre tareas.
 *
 * Implementa semaforos con cola de espera circular. Cuando una tarea
 * hace wait y el recurso no esta disponible, se bloquea y se encola.
 * Al hacer post, se despierta la primera tarea en espera (FIFO).
 * Usado para proteger regiones criticas como el acceso a la bomba,
 * el display y el logger.
 */

#include "semaphore.h"
#include "scheduler.h"
#include "pico/multicore.h"

/**
 * @brief Inicializa un semaforo con un valor dado.
 *
 * @param sem Puntero al semaforo a inicializar.
 * @param valor_inicial Cantidad inicial de recursos disponibles.
 */
void k_sem_init(kernel_semaphore_t *sem, int valor_inicial) {
    sem->contador_recursos = valor_inicial;
    sem->head = 0;
    sem->tail = 0;
}

/**
 * @brief Adquiere el semaforo (decrementa contador).
 *
 * Si el contador queda negativo, la tarea se bloquea y se agrega
 * a la cola de espera. Dispara PendSV para ceder el procesador.
 * Si la cola esta llena, revierte el decremento y retorna sin bloquear.
 *
 * @param sem Puntero al semaforo.
 */
void k_sem_wait(kernel_semaphore_t *sem) {
    int core_id = get_core_num();
    core_scheduler_t *sched = &core_schedulers[core_id];

    sem->contador_recursos--;

    if (sem->contador_recursos < 0) {
        int next_tail = (sem->tail + 1) % SEM_MAX_WAITING;
        if (next_tail == sem->head) {
            sem->contador_recursos++;
            return;
        }
        sem->tareas_esperando[sem->tail] = sched->current_task;
        sem->tail = next_tail;
        sched->tasks[sched->current_task].state = BLOCKED;
        *(volatile uint32_t *)0xE000ED04 = (1 << 28);
    }
}

/**
 * @brief Libera el semaforo (incrementa contador).
 *
 * Si hay tareas esperando (contador <= 0), despierta la primera
 * tarea en la cola FIFO marcandola como READY.
 *
 * @param sem Puntero al semaforo.
 */
void k_sem_post(kernel_semaphore_t *sem) {
    int core_id = get_core_num();
    core_scheduler_t *sched = &core_schedulers[core_id];

    sem->contador_recursos++;

    if (sem->contador_recursos <= 0) {
        if (sem->head == sem->tail) {
            return;
        }
        int next_task = sem->tareas_esperando[sem->head];
        sem->head = (sem->head + 1) % SEM_MAX_WAITING;
        sched->tasks[next_task].state = READY;
    }
}
