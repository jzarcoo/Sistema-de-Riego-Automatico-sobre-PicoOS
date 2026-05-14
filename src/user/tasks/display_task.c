#include <stdio.h>

#include "user_app.h"
#include "message_queue.h"
#include "syscalls.h"

/**
 * @brief Tarea de display que se encarga de recibir mensajes para mostrar en la pantalla OLED.
 */
void display_task(void) {
    message_t disp_msg;
    // oled_init();
    while (1) {
        sys_heartbeat();
        if (mq_receive(&display_queue, &disp_msg) == 0) {
            if (disp_msg.type == MSG_DISPLAY_TEXT) {
                // Aquí mandas el string a tu pantalla OLED real
                // oled_print(disp_msg.text); 
                sys_sem_wait(&display_sem);
                printf("[OLED DISP] %s\n", disp_msg.text);
                sys_sem_post(&display_sem);
            }
        }
    }
}