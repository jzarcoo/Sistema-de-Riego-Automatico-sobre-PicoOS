#include <stdio.h>

#include "user_app.h"
#include "message_queue.h"
#include "delay.h"
#include "syscalls.h"

#include "pico/stdlib.h"

void task(){
    message_t msg;
    msg.type = MSG_MANUAL_TRIGGER;
    mq_send(&irrigation_queue, &msg);
}

/**
 * @brief Tarea que monitorea un botón físico para activar el riego manualmente.
 * Envía un mensaje a la cola de riego cuando se detecta el botón presionado.
 * TODO: INTERRUPCION POR FLANCO DE BAJADA (FALLING EDGE) PARA EVITAR POLL Y DEBOUNCE
 */
void trigger_task(void) {
    int led_state = 0;
    int prev_btn_state = 0;
    while (1) {
        sys_heartbeat();
        int btn_state = sys_gpio_get(IRRIGATION_TRIGGER_PIN);
        // Falling edge detection for the button press
        sys_gpio_set(BUTTON_SENSOR_PIN, led_state); // Reflejar estado en el LED
        if (btn_state == 1 && prev_btn_state == 0) {
            // Debounce
            delay_cycles_exact(2);
            //sleep_ms(200);
            led_state = !led_state; 
            task();
        }
        prev_btn_state = btn_state;
    }
}