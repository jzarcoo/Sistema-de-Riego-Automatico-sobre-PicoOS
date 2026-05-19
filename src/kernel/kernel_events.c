#include "kernel_events.h"
#include "kernel_drivers.h"
#include "message_queue.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "hardware/irq.h"
#include "hardware/gpio.h"
#include <stddef.h>
#include <stdint.h>
//#include <stdio.h>

static gpio_event_entry_t gpio_events[MAX_GPIO_EVENTS];
static int gpio_event_count = 0;
static uint32_t last_irq_time = 0;

#define DEBOUNCE_MS 1000


/*
    Handler global de interrupciones GPIO.

    Esta función se ejecuta automáticamente cuando algún pin GPIO configurado genera una interrupción.

    El dispatcher:
    1. Detecta qué pin causó la interrupción
    2. Limpia el evento pendiente del hardware
    3. Aplica debounce para evitar múltiples activaciones
    4. Envía un mensaje a la cola asociada al pin
*/
static void gpio_irq_dispatcher(void) {
    uint32_t now = to_ms_since_boot(get_absolute_time());

    if ((now - last_irq_time) < DEBOUNCE_MS) {
        // Ignorar rebotes — solo limpiar la IRQ
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
            last_irq_time = now;
            if (gpio_events[i].queue != NULL) {
                message_t msg = { .type = gpio_events[i].msg_type };
                mq_send_from_isr(gpio_events[i].queue, &msg);
            }
        }
    }
}

/*
    Inicializa el sistema de eventos GPIO.

    Configura:
    - La tabla de eventos GPIO
    - El handler global de interrupciones GPIO
    - La IRQ IO_IRQ_BANK0 en la NVIC del core actual

    Después de esta función, el core podrá recibir interrupciones
    generadas por GPIO.
*/
void k_gpio_event_system_init(void) {
    gpio_event_count = 0;
    irq_set_exclusive_handler(IO_IRQ_BANK0, gpio_irq_dispatcher);
    irq_set_enabled(IO_IRQ_BANK0, true);
   // printf("[IRQ_SYS] Handler instalado en core %d\n", get_core_num());
}

/*
    Registra un GPIO como fuente de eventos.

    El pin debe estar previamente configurado (dirección, pull-up/down).
    Esta función solo asocia el pin con una cola y habilita la IRQ.

    @param pin GPIO a monitorear (ya configurado como input)
    @param edge Tipo de edge (GPIO_IRQ_EDGE_RISE, GPIO_IRQ_EDGE_FALL)
    @param queue Cola que recibirá el mensaje
    @param msg_type Tipo de mensaje a enviar
*/
void k_gpio_event_register(int pin, uint32_t edge, message_queue_t *queue, message_type_t msg_type) {
    if (gpio_event_count >= MAX_GPIO_EVENTS) return;

    gpio_events[gpio_event_count].pin = pin;
    gpio_events[gpio_event_count].queue = queue;
    gpio_events[gpio_event_count].msg_type = msg_type;
    gpio_event_count++;

    gpio_set_irq_enabled(pin, edge, true);
}
