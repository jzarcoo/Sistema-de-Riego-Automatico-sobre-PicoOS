#ifndef USER_APP_H
#define USER_APP_H

#include "semaphore.h"
#include "message_queue.h"

// Tareas para el Scheduler
#define IRRIGATION_TASK_ID      0
#define SENSOR_TASK_ID          1
#define TRIGGER_TASK_ID         2
#define LOGGER_TASK_ID          3
#define DISPLAY_TASK_ID         4

// Pines físicos asignados a la aplicación
// Botón físico para iniciar el riego
#define IRRIGATION_TRIGGER_PIN     16
// Pin para controlar la bomba de agua
#define IRRIGATION_PUMP_PIN        17
// Pin para el sensor de humedad del suelo
#define SOIL_MOISTURE_SENSOR_PIN   26

// Semáforos
extern semaphore_t irrigation_pump_sem;
extern semaphore_t logger_sem;
extern semaphore_t display_sem;

// Cola de mensajes 
extern message_queue_t irrigation_queue;
extern message_queue_t log_queue;
extern message_queue_t display_queue;

// Prototipos de las funciones de tareas de usuario
void irrigation_task(void);
void sensor_task(void);
void trigger_task(void);
void logger_task(void);
void display_task(void);

// Inicializador para registrar las aplicaciones de usuario en el Kernel
void setup_irrigation(void);
void user_app_init(void);

#endif // USER_APP_H