/**
 * @file irrigation_task.c
 * @brief Tarea consumidora de la cola de riego.
 *
 * Envia a display_queue (fila 1 = bomba, fila 2 = evento)
 * y a log_queue. Corre en Core 1 (plano critico).
 */

#include <string.h>

#include "user_app.h"
#include "message_queue.h"
#include "syscalls.h"
#include "display_manager.h"

static void display_row(int row, const char *text) {
    message_t msg;
    msg.type = MSG_DISPLAY_TEXT;
    msg.data = row;
    strncpy(msg.text, text, sizeof(msg.text));
    mq_send(&display_queue, &msg);
}

static void drain_triggers(void) {
    message_t discard;
    while (mq_receive(&irrigation_queue, &discard) == 0) {
        if (discard.type == MSG_MANUAL_TRIGGER) continue;
        mq_send(&irrigation_queue, &discard);
        break;
    }
}

static void perform_irrigation(void) {
    sys_print("[BOMBA] ON\n");
    display_row(DISPLAY_ROW_PUMP, "BOMBA: ON");
    display_row(DISPLAY_ROW_EVENT1, "Regando...");

    sys_sem_wait(&irrigation_pump_sem);
    sys_request_irrigation();

    sys_sleep(4000);

    sys_sem_post(&irrigation_pump_sem);

    sys_print("[BOMBA] OFF\n");
    display_row(DISPLAY_ROW_PUMP, "BOMBA: OFF");
    display_row(DISPLAY_ROW_EVENT1, "Riego completo.");

    /* Drenar triggers falsos acumulados por EMI del relay */
    drain_triggers();
    /* Ventana de 5s post-bomba: ignorar triggers (ruido residual) */
    sys_sleep(5000);
    drain_triggers();
}

void irrigation_task(void) {
    sys_print("[CORE1] Irrigation Task iniciada.\n");
    display_row(DISPLAY_ROW_PUMP, "BOMBA: OFF");

    message_t msg;
    message_t out_msg;

    while (1) {
        sys_heartbeat();

        if (mq_receive(&irrigation_queue, &msg) == 0) {
            switch (msg.type) {
                case MSG_SOIL_DRY:
                    sys_print("[RIEGO] Suelo seco -> activando bomba\n");
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
                    sys_print("[RIEGO] Trigger manual -> activando bomba\n");
                    out_msg.type = MSG_LOG_TEXT;
                    strncpy(out_msg.text, "Riego manual.",
                            sizeof(out_msg.text));
                    mq_send(&log_queue, &out_msg);
                    display_row(DISPLAY_ROW_EVENT1, "Riego manual.");
                    perform_irrigation();
                    break;

                default:
                    break;
            }
        }
    }
}
