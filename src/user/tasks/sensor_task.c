#include <stdio.h>

#include "user_app.h"
#include "message_queue.h"
#include "delay.h"
#include "syscalls.h"

/**
 * @brief Tarea que monitorea el estado del sensor de humedad del suelo. 
 * Envía mensajes a la cola de riego cuando detecta cambios en el estado del suelo (seco/húmedo).
 */
void sensor_task(void) {
    message_t msg;
    while (1) {
        sys_heartbeat();
        int sensor_state = sys_read_soil_sensor();
        printf("Sensor de humedad: %d\n", sensor_state);

        // if (sensor_state == 0) {
        //     msg.type = MSG_SOIL_DRY;
        //     mq_send(&irrigation_queue, &msg);
        // } else {
        //     msg.type = MSG_SOIL_WET;
        //     mq_send(&irrigation_queue, &msg);
        // }
        // delay_cycles_exact(500000); 
    }
}