#include <stdio.h>

#include "user_app.h"
#include "message_queue.h"
#include "delay.h"
#include "syscalls.h"

/**
 * @brief Tarea que monitorea un botón físico para activar el riego manualmente.
 * Envía un mensaje a la cola de riego cuando se detecta el botón presionado.
 */
void trigger_task(void) {
    while (1) {
        sys_heartbeat();
        if (sys_gpio_get(IRRIGATION_TRIGGER_PIN) == 0) {
            message_t msg;
            msg.type = MSG_MANUAL_TRIGGER;
            mq_send(&irrigation_queue, &msg);
            //delay_cycles_exact(1000000); // Antidebote (Debounce)
        }
    }
}