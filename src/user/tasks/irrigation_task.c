/**
 * @file irrigation_task.c
 * @brief Tarea consumidora de la cola de riego.
 *
 * Recibe mensajes de irrigation_queue y actua:
 * - MSG_SOIL_DRY / MSG_MANUAL_TRIGGER: inicia ciclo de riego.
 * - MSG_SOIL_WET: actualiza display con estado normal.
 *
 * Corre en Core 1 (plano critico).
 */

#include <stdio.h>
#include <string.h>

#include "user_app.h"
#include "message_queue.h"
#include "syscalls.h"

static void perform_irrigation(void) {
    message_t out_msg;

    out_msg.type = MSG_DISPLAY_TEXT;
    strncpy(out_msg.text, "REGANDO...", sizeof(out_msg.text));
    mq_send(&display_queue, &out_msg);

    sys_sem_wait(&irrigation_pump_sem);
    sys_request_irrigation();
    sys_sem_post(&irrigation_pump_sem);

    out_msg.type = MSG_DISPLAY_TEXT;
    strncpy(out_msg.text, "RIEGO COMPLETO", sizeof(out_msg.text));
    mq_send(&display_queue, &out_msg);
}

void irrigation_task(void) {
    sys_print("[Irrigation Task] Iniciada.\n");

    message_t msg;
    message_t out_msg;

    while (1) {
        sys_heartbeat();

        if (mq_receive(&irrigation_queue, &msg) == 0) {
            switch (msg.type) {
                case MSG_SOIL_DRY:
                    out_msg.type = MSG_LOG_TEXT;
                    strncpy(out_msg.text, "Suelo seco. Regando...",
                            sizeof(out_msg.text));
                    mq_send(&log_queue, &out_msg);
                    perform_irrigation();
                    break;

                case MSG_SOIL_WET:
                    out_msg.type = MSG_LOG_TEXT;
                    strncpy(out_msg.text, "Humedad normal.",
                            sizeof(out_msg.text));
                    mq_send(&log_queue, &out_msg);
                    break;

                case MSG_MANUAL_TRIGGER:
                    out_msg.type = MSG_LOG_TEXT;
                    strncpy(out_msg.text, "Riego manual activado.",
                            sizeof(out_msg.text));
                    mq_send(&log_queue, &out_msg);
                    perform_irrigation();
                    break;

                default:
                    break;
            }
        }
    }
}
