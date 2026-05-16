#include <stdio.h>

#include "user_app.h"
#include "message_queue.h"
#include "delay.h"
#include "syscalls.h"

#include "pico/stdlib.h"

/**
 * @brief Tarea de disparo manual.
 * Mantiene el sistema activo con latidos y no utiliza la bandera global de IRQ.
 */
void trigger_task(void) {
    while (1) {
        sys_heartbeat();
    }
}

