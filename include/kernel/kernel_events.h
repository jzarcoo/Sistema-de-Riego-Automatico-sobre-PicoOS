/**
 * @file kernel_events.h
 * @brief Subsistema de eventos GPIO por interrupciones.
 *
 * Permite asociar pines GPIO con colas de mensajes para
 * despachar eventos de hardware (flancos) a las tareas.
 */

#ifndef KERNEL_EVENTS_H
#define KERNEL_EVENTS_H

#include "message_queue.h"

/** Maximo de eventos GPIO que se pueden registrar */
#define MAX_GPIO_EVENTS 4

/**
 * @brief Entrada en la tabla de eventos GPIO.
 * Asocia un pin con una cola y tipo de mensaje.
 */
typedef struct {
    int pin;                    /**< Numero de GPIO */
    message_queue_t *queue;     /**< Cola destino del mensaje */
    message_type_t msg_type;    /**< Tipo de mensaje a enviar */
} gpio_event_entry_t;

/** Inicializa el subsistema de eventos e instala el IRQ handler */
void k_gpio_event_system_init(void);

/** Registra un pin como fuente de eventos con edge e cola destino */
void k_gpio_event_register(int pin, uint32_t edge, message_queue_t *queue, message_type_t msg_type);

#endif
