#include "scheduler.h"
#include "user_app.h"
#include "syscalls.h"

semaphore_t irrigation_pump_sem;
semaphore_t logger_sem;
semaphore_t display_sem;

message_queue_t irrigation_queue;
message_queue_t log_queue;
message_queue_t display_queue;

/**
 * @brief Configura los pines GPIO y el semáforo necesario para la tarea de riego.
 */
void setup_irrigation(void){
    // Configurar el pin del botón como entrada (con pull-up)
    sys_gpio_dir(IRRIGATION_TRIGGER_PIN, 0);
    // TODO: k_gpio_pullup(IRRIGATION_TRIGGER_PIN);

    // Configurar el pin de la bomba como salida 
    sys_gpio_dir(IRRIGATION_PUMP_PIN, 1);

    // Configurar el pin del led como salida
    sys_gpio_dir(BUTTON_SENSOR_PIN, 1);

    // Configurar el pin del sensor de humedad como entrada
    //sys_gpio_dir(SOIL_MOISTURE_SENSOR_PIN, 0);
    sys_adc_init();
    
    // Inicializar el semáforo para la bomba con 1 recurso disponible
    sys_sem_init(&irrigation_pump_sem, 1);

    sys_sem_init(&logger_sem, 1);
    sys_sem_init(&display_sem, 1);


    // Inicializar la cola de mensajes para la aplicación de riego
    mq_init(&irrigation_queue);

    // Inicializar la cola de mensajes para el logger
    mq_init(&log_queue);

    // Inicializar la cola de mensajes para el display
    mq_init(&display_queue);
}

/**
 * @brief Inicializa y registra todas las tareas de usuario en el planificador.
 */
void user_app_init(void) {
    setup_irrigation();

    task_create(IRRIGATION_TASK_ID, irrigation_task);
    task_create(SENSOR_TASK_ID, sensor_task);
    task_create(TRIGGER_TASK_ID, trigger_task);
    task_create(LOGGER_TASK_ID, logger_task);
    task_create(DISPLAY_TASK_ID, display_task);
}