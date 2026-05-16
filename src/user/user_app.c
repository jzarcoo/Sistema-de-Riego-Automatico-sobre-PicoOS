#include "user_app.h"
#include "syscalls.h"
#include "message_queue.h"
#include<stdio.h>

semaphore_t irrigation_pump_sem;
semaphore_t logger_sem;
semaphore_t display_sem;

message_queue_t irrigation_queue;
message_queue_t log_queue;
message_queue_t display_queue;

void setup_irrigation(void){
    sys_gpio_dir(USER_TRIGGER_PIN, 0);
    sys_gpio_pullup(USER_TRIGGER_PIN);
    sys_gpio_irq_init(USER_TRIGGER_PIN);

    sys_gpio_dir(BUTTON_SENSOR_PIN, 1);

    sys_adc_init();

    sys_sem_init(&irrigation_pump_sem, 1);
    sys_sem_init(&logger_sem, 1);
    sys_sem_init(&display_sem, 1);

    mq_init(&irrigation_queue);
    sys_manual_trigger_event_init(&irrigation_queue);

    mq_init(&log_queue);
    mq_init(&display_queue);
}

/**
 * prueba todo: borrar
 */
void attacker_task(void) {

    printf("[ATTACKER] Intentando acceder GPIO protegido...\n");

    volatile uint32_t *gpio =
        (uint32_t *)0xD0000000;

    *gpio = 0xDEADBEEF;

    while (1){
        sys_heartbeat();
    }
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
    task_create(5, attacker_task); // Tarea atacante para probar seguridad (intenta escribir en GPIO protegido)
}
