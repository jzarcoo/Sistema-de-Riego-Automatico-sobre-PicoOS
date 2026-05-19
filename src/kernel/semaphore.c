#include "semaphore.h"
#include "scheduler.h"
#include "pico/multicore.h"

void k_sem_init(kernel_semaphore_t *sem, int valor_inicial) {
    sem->contador_recursos = valor_inicial;
    sem->head = 0;
    sem->tail = 0;
}

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
