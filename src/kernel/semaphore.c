#include "semaphore.h"
#include "scheduler.h"

extern tcb_t tasks[MAX_TASKS];
extern int current_task;

void k_sem_init(semaphore_t *sem, int valor_inicial) {
    sem->contador_recursos = valor_inicial;
    sem->head = 0;
    sem->tail = 0;
}

void k_sem_wait(semaphore_t *sem) {
    sem->contador_recursos--;

    if (sem->contador_recursos < 0) {
        int next_tail = (sem->tail + 1) % SEM_MAX_WAITING;
        if (next_tail == sem->head) {
            sem->contador_recursos++;
            return;
        }
        sem->tareas_esperando[sem->tail] = current_task;
        sem->tail = next_tail;
        tasks[current_task].state = BLOCKED;
        *(volatile uint32_t *)0xE000ED04 = (1 << 28);
    }
}

void k_sem_post(semaphore_t *sem) {
    sem->contador_recursos++;

    if (sem->contador_recursos <= 0) {
        if (sem->head == sem->tail) {
            return;
        }
        int next_task = sem->tareas_esperando[sem->head];
        sem->head = (sem->head + 1) % SEM_MAX_WAITING;
        tasks[next_task].state = READY;
    }
}
