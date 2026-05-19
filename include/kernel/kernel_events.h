#ifndef KERNEL_EVENTS_H
#define KERNEL_EVENTS_H

#include "message_queue.h"

#define MAX_GPIO_EVENTS 4

/*
    Estructura de un mensaje de evento de GPIO
*/
typedef struct {
    int pin;
    message_queue_t *queue;
    message_type_t msg_type;
} gpio_event_entry_t;


// Inicializacion del subsistema de eventos
void k_gpio_event_system_init(void);

// Inicializacion de un pin como generador de eventos
void k_gpio_event_register(int pin, uint32_t edge, message_queue_t *queue, message_type_t msg_type);

#endif
