#include <stdio.h>
#include "hardware/gpio.h"
#include "user_app.h"
#include "message_queue.h"
#include "syscalls.h"

/**
 * @brief Tarea de display que se encarga de recibir mensajes para mostrar en la pantalla OLED.
 */
void display_task(void) {
    message_t disp_msg;
    while (1) {

        gpio_put(6, 1);
        sys_heartbeat();
        if (mq_receive(&display_queue, &disp_msg) == 0) {
            if (disp_msg.type == MSG_DISPLAY_TEXT) {
                sys_sem_wait(&display_sem);
                char buf[48];
                snprintf(buf, sizeof(buf), "[OLED DISP] %s\n", disp_msg.text);
                sys_print(buf);
                sys_sem_post(&display_sem);
            }
        }
    }
}