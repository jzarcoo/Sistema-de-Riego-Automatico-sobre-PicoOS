/**
 * @file display_task.c
 * @brief Tarea del display LCD (user space).
 *
 * Consume mensajes de display_queue, escribe la fila indicada
 * (msg.data = numero de fila) y hace flush. Protegido por display_sem.
 *
 * Corre en Core 0 (plano de gestion/UI).
 */

#include "user_app.h"
#include "message_queue.h"
#include "syscalls.h"

void display_task(void) {
    sys_print("[CORE0] Display Task iniciada.\n");
    message_t msg;

    while (1) {
        sys_heartbeat();

        if (mq_receive(&display_queue, &msg) == 0) {
            if (msg.type == MSG_DISPLAY_TEXT) {
                sys_sem_wait(&display_sem);
                sys_display_write(msg.data, msg.text);
                sys_display_flush();
                sys_sem_post(&display_sem);
            }
        }
    }
}
