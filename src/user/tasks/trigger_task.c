/**
 * @file trigger_task.c
 * @brief Tarea de riego manual por boton fisico.
 *
 * Esta tarea mantiene el heartbeat activo mientras el subsistema
 * de eventos GPIO se encarga de detectar la pulsacion del boton.
 * Cuando el usuario presiona el boton (GPIO 14), el ISR de
 * kernel_events envia MSG_MANUAL_TRIGGER directamente a la cola
 * irrigation_queue. Esta tarea solo necesita estar viva para que
 * el watchdog no la reinicie innecesariamente.
 *
 * Corre en Core 1 (plano de control critico).
 */

#include <stdio.h>

#include "user_app.h"
#include "message_queue.h"
#include "syscalls.h"

/**
 * @brief Loop principal de la tarea de trigger manual.
 * Reporta heartbeat al kernel continuamente.
 */
void trigger_task(void) {
    while (1) {
        sys_heartbeat();
    }
}
