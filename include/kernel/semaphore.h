/**
 * @file semaphore.h
 * @brief Definicion de semaforos de conteo con cola de espera FIFO.
 *
 * Provee exclusion mutua entre tareas del mismo core.
 * La cola circular almacena hasta SEM_MAX_WAITING tareas bloqueadas.
 */

#ifndef SEMAPHORE_H
#define SEMAPHORE_H

#include <stdint.h>

/** Maximo de tareas que pueden esperar en un semaforo */
#define SEM_MAX_WAITING 10

/**
 * @brief Estructura del semaforo de kernel.
 *
 * Usa un contador de recursos y una cola circular FIFO
 * para las tareas que esperan cuando el recurso no esta disponible.
 */
typedef struct {
    int contador_recursos;                  /** Recursos disponibles (negativo = tareas esperando) */
    int tareas_esperando[SEM_MAX_WAITING];  /** Cola circular de IDs de tareas bloqueadas */
    int head;                               /** Indice de lectura de la cola */
    int tail;                               /** Indice de escritura de la cola */
} kernel_semaphore_t;

/** Inicializa un semaforo con valor inicial */
void k_sem_init(kernel_semaphore_t *sem, int valor_inicial);

/** Adquiere el semaforo (bloquea si no hay recursos) */
void k_sem_wait(kernel_semaphore_t *sem);

/** Libera el semaforo (despierta una tarea si hay esperando) */
void k_sem_post(kernel_semaphore_t *sem);

#endif
