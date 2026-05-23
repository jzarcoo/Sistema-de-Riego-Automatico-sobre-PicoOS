/**
 * @file sensor_task.c
 * @brief Tarea de lectura periodica del sensor de humedad.
 *
 * Lee ADC cada 5s, convierte a %, envia a display_queue (fila 0)
 * y a irrigation_queue segun umbral.
 * Corre en Core 1 (plano critico).
 */

#include <stdio.h>
#include <string.h>

#include "user_app.h"
#include "message_queue.h"
#include "syscalls.h"
#include "display_manager.h"

#define ADC_DRY_VALUE  4100
#define ADC_WET_VALUE  1800
#define IRRIGATION_THRESHOLD 2500

static int adc_to_percent(int adc_value) {
    if (adc_value >= ADC_DRY_VALUE) return 0;
    if (adc_value <= ADC_WET_VALUE) return 100;
    return (ADC_DRY_VALUE - adc_value) * 100 / (ADC_DRY_VALUE - ADC_WET_VALUE);
}

static const char *humidity_status(int adc_value) {
    if (adc_value > 3500) return "MUY SECO";
    if (adc_value > IRRIGATION_THRESHOLD) return "SECO";
    if (adc_value > ADC_WET_VALUE) return "NORMAL";
    return "MUY HUMEDO";
}

void sensor_task(void) {
    sys_print("[CORE1] Sensor Task iniciada.\n");
    message_t msg;
    char buf[32];

    while (1) {
        sys_heartbeat();
        int raw = sys_read_soil_sensor();
        int percent = adc_to_percent(raw);

        /* Enviar a display_queue: fila 0 = humedad */
        msg.type = MSG_DISPLAY_TEXT;
        msg.data = DISPLAY_ROW_HUMIDITY;
        snprintf(msg.text, sizeof(msg.text), "HUM:%d%% %s", percent, humidity_status(raw));
        mq_send(&display_queue, &msg);

        /* UART debug */
        snprintf(buf, sizeof(buf), "Sensor: %d%% (ADC: %d)\n", percent, raw);
        sys_print(buf);

        /* Enviar a irrigation_queue */
        if (raw > IRRIGATION_THRESHOLD) {
            msg.type = MSG_SOIL_DRY;
            mq_send(&irrigation_queue, &msg);
        } else {
            msg.type = MSG_SOIL_WET;
            mq_send(&irrigation_queue, &msg);
        }

        sys_sleep(5000);
    }
}
