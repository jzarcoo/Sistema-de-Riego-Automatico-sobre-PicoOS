#ifndef SEMAPHORE_H
#define SEMAPHORE_H

#include <stdint.h>
#include "scheduler.h"

// Estructura de datos para semáforos
typedef struct {
    int contador_recursos;              // Contador de recursos disponibles
    int tareas_esperando[MAX_TASKS];    // Cola circular de tareas bloqueadas esperando el semáforo
    int head;                           // Índice de la cabeza de la cola
    int tail;                           // Índice de la cola 
} semaphore_t;

// Prototipos de funciones de semáforo
void k_sem_init(semaphore_t *sem, int valor_inicial);
void k_sem_wait(semaphore_t *sem);
void k_sem_post(semaphore_t *sem);

#endif 