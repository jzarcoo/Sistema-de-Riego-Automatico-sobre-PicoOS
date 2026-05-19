#ifndef USER_APP_H
#define USER_APP_H

#include "semaphore.h"
#include "message_queue.h"

// IDs de tareas (para referencia)
#define IRRIGATION_TASK_ID      0
#define SENSOR_TASK_ID          1
#define TRIGGER_TASK_ID         2
#define LOGGER_TASK_ID          3
#define DISPLAY_TASK_ID         4

// Pines visibles para tareas de usuario (configurados via syscalls)
#define BUTTON_SENSOR_PIN       25
#define USER_TRIGGER_PIN        16

// Semaforos compartidos entre tareas de usuario
extern kernel_semaphore_t irrigation_pump_sem;
extern kernel_semaphore_t logger_sem;
extern kernel_semaphore_t display_sem;

// Colas de mensajes compartidas entre tareas de usuario
extern message_queue_t irrigation_queue;
extern message_queue_t log_queue;
extern message_queue_t display_queue;

// Prototipos de las funciones de tareas de usuario
void irrigation_task(void);
void sensor_task(void);
void trigger_task(void);
void logger_task(void);
void display_task(void);

// Inicializacion del hardware de usuario (llamada desde la primera tarea)
void setup_irrigation(void);

#endif // USER_APP_H
