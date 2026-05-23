/**
 * @file display_task.c
 * @brief Tarea de actualizacion del display OLED (user space).
 *
 * Consume mensajes MSG_DISPLAY_TEXT de display_queue y solicita
 * al kernel que actualice la pantalla via syscall.
 * Corre en Core 0 (plano de gestion/UI).
 */

#include <stdio.h>
#include "user_app.h"
#include "message_queue.h"
#include "syscalls.h"

void display_task(void) {
    message_t disp_msg;
    while (1) {
        sys_heartbeat();
        if (mq_receive(&display_queue, &disp_msg) == 0) {
            if (disp_msg.type == MSG_DISPLAY_TEXT) {
                sys_sem_wait(&display_sem);
                sys_request_display_update(disp_msg.text);
                sys_sem_post(&display_sem);
            }
        }
    }
}
