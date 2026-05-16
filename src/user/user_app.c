#include "user_app.h"
#include "syscalls.h"
#include "message_queue.h"

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
