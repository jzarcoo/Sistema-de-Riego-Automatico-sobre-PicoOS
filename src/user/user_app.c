#include "user_app.h"
#include "syscalls.h"
#include "message_queue.h"
#include "scheduler.h"
#include <stdio.h>

kernel_semaphore_t irrigation_pump_sem;
kernel_semaphore_t logger_sem;
kernel_semaphore_t display_sem;

message_queue_t irrigation_queue;
message_queue_t log_queue;
message_queue_t display_queue;

void setup_irrigation(void){
    sys_gpio_dir(BUTTON_SENSOR_PIN, 1);

    sys_adc_init();

    sys_sem_init(&irrigation_pump_sem, 1);
    sys_sem_init(&logger_sem, 1);
    sys_sem_init(&display_sem, 1);

    mq_init(&log_queue);
    mq_init(&display_queue);
}
