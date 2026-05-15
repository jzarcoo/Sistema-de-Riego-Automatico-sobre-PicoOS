#include <stdio.h>
#include <string.h> 
#include "pico/stdlib.h"

#include "user_app.h"
#include "message_queue.h"
#include "syscalls.h"
#include "delay.h"

/**
 * @brief Ejecuta el proceso de riego.
 * 
 * Enciende la bomba usando un semáforo para exclusión mutua
 * y actualiza el display durante el proceso.
 */
static void perform_irrigation(void) {
    message_t out_msg;

    out_msg.type = MSG_DISPLAY_TEXT;
    strncpy(out_msg.text, "MODO: REGANDO", sizeof(out_msg.text));
    mq_send(&display_queue, &out_msg);

    sys_sem_wait(&irrigation_pump_sem);

    sys_pump_on();
    sleep_ms(1000); 
    sys_pump_off();

    sys_sem_post(&irrigation_pump_sem);

    // Volver a modo espera
    out_msg.type = MSG_DISPLAY_TEXT;
    strncpy(out_msg.text, "MODO: ESPERA", sizeof(out_msg.text));
    mq_send(&display_queue, &out_msg);
}

/**
 * @brief Tarea de riego.
 */
void irrigation_task(void) {
    message_t msg;
    message_t out_msg; 

    while (1) {
        sys_heartbeat();

        if (mq_receive(&irrigation_queue, &msg) == 0) {

            switch (msg.type) {

                case MSG_SOIL_DRY:

                    out_msg.type = MSG_LOG_TEXT;
                    strncpy(out_msg.text,
                            "Suelo seco. Regando...",
                            sizeof(out_msg.text));

                    mq_send(&log_queue, &out_msg);

                    perform_irrigation();
                    break;

                case MSG_SOIL_WET:

                    out_msg.type = MSG_LOG_TEXT;
                    strncpy(out_msg.text,
                            "Suelo Humedo.",
                            sizeof(out_msg.text));

                    mq_send(&log_queue, &out_msg);

                    out_msg.type = MSG_DISPLAY_TEXT;
                    strncpy(out_msg.text,
                            "Suelo: Humedo.",
                            sizeof(out_msg.text));

                    mq_send(&display_queue, &out_msg);

                    break;

                case MSG_MANUAL_TRIGGER:

                    out_msg.type = MSG_LOG_TEXT;
                    strncpy(out_msg.text,
                            "Riego Manual.",
                            sizeof(out_msg.text));

                    mq_send(&log_queue, &out_msg);

                    perform_irrigation();
                    break;
            }
        }
    }
}