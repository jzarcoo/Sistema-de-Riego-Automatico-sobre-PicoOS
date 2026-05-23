/**
 * @file sensor_task.c
 * @brief Tarea de lectura periodica del sensor de humedad.
 *
 * Lee el ADC cada 5 segundos, convierte a porcentaje, muestra
 * estado en display y envia mensaje a la cola de riego.
 *
 * Escala ADC: ~4100 = 0% (aire/seco), ~1800 = 100% (agua/saturado).
 * Ajustar umbrales segun el tipo de tierra.
 *
 * Corre en Core 1 (plano critico).
 */

#include <stdio.h>
#include <string.h>

#include "user_app.h"
#include "message_queue.h"
#include "syscalls.h"

/* Calibracion del sensor (ajustar segun tierra) */
#define ADC_DRY_VALUE  4100
#define ADC_WET_VALUE  1800

/* Umbral para activar riego automatico */
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
    message_t msg;
    char buf[48];

    while (1) {
        sys_heartbeat();
        int raw = sys_read_soil_sensor();
        int percent = adc_to_percent(raw);

        snprintf(buf, sizeof(buf), "HUMEDAD: %d%%\n%s", percent, humidity_status(raw));

        msg.type = MSG_DISPLAY_TEXT;
        strncpy(msg.text, buf, sizeof(msg.text));
        mq_send(&display_queue, &msg);

        snprintf(buf, sizeof(buf), "Sensor: %d%% (ADC: %d)\n", percent, raw);
        sys_print(buf);

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
