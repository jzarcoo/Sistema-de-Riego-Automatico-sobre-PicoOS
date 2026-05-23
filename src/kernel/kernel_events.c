/**
 * @file kernel_events.c
 * @brief Subsistema de eventos GPIO por interrupciones de hardware.
 *
 * Permite registrar pines GPIO como fuentes de eventos. Cuando un
 * pin genera una interrupcion (rising/falling edge), el dispatcher
 * global envia un mensaje tipado a la cola asociada. Incluye
 * debounce por software para evitar activaciones multiples por
 * rebote mecanico del boton.
 *
 * Arquitectura: El handler se instala una vez por core y despacha
 * a todas las colas registradas segun el pin que genero la IRQ.
 */

#include "kernel_events.h"
#include "kernel_drivers.h"
#include "message_queue.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "hardware/irq.h"
#include "hardware/gpio.h"
#include <stddef.h>
#include <stdint.h>

/** Tabla de eventos GPIO registrados */
static gpio_event_entry_t gpio_events[MAX_GPIO_EVENTS];

/** Cantidad de eventos actualmente registrados */
static int gpio_event_count = 0;

/** Timestamp del ultimo evento procesado (para debounce) */
static uint32_t last_irq_time = 0;

/** Ventana de debounce en milisegundos */
#define DEBOUNCE_MS 1000

/**
 * @brief Handler global de interrupciones GPIO (ISR).
 *
 * Se ejecuta automaticamente cuando un pin configurado genera IRQ.
 * Recorre la tabla de eventos, identifica el pin activo, aplica
 * debounce temporal, y envia el mensaje correspondiente a la cola.
 * Usa mq_send_from_isr() que es safe para contexto de interrupcion.
 */
static void gpio_irq_dispatcher(void) {
    uint32_t now = to_ms_since_boot(get_absolute_time());

    if ((now - last_irq_time) < DEBOUNCE_MS) {
        for (int i = 0; i < gpio_event_count; i++) {
            uint32_t events = gpio_get_irq_event_mask(gpio_events[i].pin);
            if (events) gpio_acknowledge_irq(gpio_events[i].pin, events);
        }
        return;
    }

    for (int i = 0; i < gpio_event_count; i++) {
        int pin = gpio_events[i].pin;
        uint32_t events = gpio_get_irq_event_mask(pin);
        if (events) {
            gpio_acknowledge_irq(pin, events);
            /* Confirmar que el pin sigue en HIGH (pulsacion real vs ruido) */
            if (!gpio_get(pin)) continue;
            last_irq_time = now;
            if (gpio_events[i].queue != NULL) {
                message_t msg = { .type = gpio_events[i].msg_type };
                mq_send_from_isr(gpio_events[i].queue, &msg);
            }
        }
    }
}

/**
 * @brief Inicializa el subsistema de eventos GPIO.
 *
 * Resetea la tabla de eventos, instala el handler global en la
 * IRQ IO_IRQ_BANK0 del core actual, y habilita la IRQ en la NVIC.
 * Debe llamarse una vez por core antes de registrar eventos.
 */
void k_gpio_event_system_init(void) {
    gpio_event_count = 0;
    irq_set_exclusive_handler(IO_IRQ_BANK0, gpio_irq_dispatcher);
    irq_set_enabled(IO_IRQ_BANK0, true);
}

/**
 * @brief Registra un pin GPIO como fuente de eventos.
 *
 * El pin debe estar previamente configurado (direccion, pull-up/down).
 * Asocia el pin con una cola de mensajes y habilita la IRQ del edge.
 *
 * @param pin Numero de GPIO (ya configurado como input).
 * @param edge Tipo de flanco (GPIO_IRQ_EDGE_RISE o GPIO_IRQ_EDGE_FALL).
 * @param queue Cola que recibira el mensaje cuando ocurra el evento.
 * @param msg_type Tipo de mensaje a enviar (ej. MSG_MANUAL_TRIGGER).
 */
void k_gpio_event_register(int pin, uint32_t edge, message_queue_t *queue, message_type_t msg_type) {
    if (gpio_event_count >= MAX_GPIO_EVENTS) return;

    gpio_events[gpio_event_count].pin = pin;
    gpio_events[gpio_event_count].queue = queue;
    gpio_events[gpio_event_count].msg_type = msg_type;
    gpio_event_count++;

    gpio_set_irq_enabled(pin, edge, true);
}
