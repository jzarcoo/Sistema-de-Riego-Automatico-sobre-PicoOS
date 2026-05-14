#include "semaphore.h"
#include "scheduler.h"

extern tcb_t tasks[MAX_TASKS];
extern int current_task;

/**
 * @brief Initialize a semaphore with a given initial value.
 * @param sem Pointer to the semaphore to initialize.
 * @param valor_inicial Initial value of the semaphore.
 */
void k_sem_init(semaphore_t *sem, int valor_inicial) {
    sem->contador_recursos = valor_inicial;
    sem->head = 0;
    sem->tail = 0;
}

/**
 * @brief Wait (decrement) operation on the semaphore. If the resource is not available, block the task.
 * @param sem Pointer to the semaphore to wait on.
 */
void k_sem_wait(semaphore_t *sem) {
    sem->contador_recursos--;

    if (sem->contador_recursos < 0) {
        
        int next_tail = (sem->tail + 1) % MAX_TASKS;
        if (next_tail == sem->head) {
            // Queue llena
            sem->contador_recursos++; 
            return;
        }
        sem->tareas_esperando[sem->tail] = current_task;
        sem->tail = next_tail;
        tasks[current_task].state = BLOCKED;
        // Trigger PendSV to perform the context switch
        *(volatile uint32_t *)0xE000ED04 = (1 << 28);
    }
}


/**
 * @brief Post (increment) operation on the semaphore. If there are tasks waiting, unblock one of them.
 * @param sem Pointer to the semaphore to post on.
 */
void k_sem_post(semaphore_t *sem) {
    sem->contador_recursos++;

    if (sem->contador_recursos <= 0) {
        if (sem->head == sem->tail) {
            // No hay tareas esperando
            return;
        }
        int next_task = sem->tareas_esperando[sem->head];
        sem->head = (sem->head + 1) % MAX_TASKS;
        tasks[next_task].state = READY;
    }
}