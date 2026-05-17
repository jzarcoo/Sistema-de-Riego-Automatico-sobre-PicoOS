#include <stdio.h>

#include "user_app.h"
#include "message_queue.h"
#include "syscalls.h"

void sensor_task(void) {
    message_t msg;
    int umbral = 2500;
    while (1) {
        sys_heartbeat();
        int sensor_state = sys_read_soil_sensor();

        printf("Sensor de humedad: %d\n", sensor_state);

        if (sensor_state > umbral) {
            msg.type = MSG_SOIL_DRY;
            mq_send(&irrigation_queue, &msg);
        } else {
            msg.type = MSG_SOIL_WET;
            mq_send(&irrigation_queue, &msg);
        }

        sys_sleep(5000);
    }
}