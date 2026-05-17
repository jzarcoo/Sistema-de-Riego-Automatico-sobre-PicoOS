#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdint.h>

#define MAX_TASKS 10
#define STACK_SIZE 256 // 256 palabras de 32 bits = 1KB por tarea

#define TASK_TIMEOUT_TICKS 500

// Estados posibles de una tarea
typedef enum {
    DORMANT, // Inactiva, no está en la cola
    READY,   // Lista para ejecutarse
    RUNNING,  // Ejecutándose actualmente
    BLOCKED   // Bloqueada, esperando un recurso
} task_state_t;

// Task Control Block
typedef struct {
    uint32_t *sp;                  // Puntero de Pila actual
    task_state_t state;            // Estado de la tarea
    uint32_t stack[STACK_SIZE];    // Espacio de memoria de la pila
    int quantum;                   // Quantum asignado
    int remaining_ticks;           // Ticks restantes
    void (*entry_point)(void);     // Función de la tarea

    uint32_t heartbeat;
    uint32_t last_seen;
    uint32_t wake_tick;            // Tick en el que debe despertar (0 = no duerme)
} tcb_t;



extern tcb_t tasks[MAX_TASKS];
extern int current_task;
extern volatile uint32_t kernel_ticks;



// Prototipos del planificador
void scheduler_init(void);
void task_create(int id, void (*entry_point)(void));
void k_task_exit(void);
void k_task_sleep(uint32_t ms);
uint32_t schedule(uint32_t current_sp);

#endif