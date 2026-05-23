/**
 * @file message_queue.h
 * @brief Colas de mensajes tipados para IPC entre tareas.
 *
 * Implementa un buffer circular de mensajes con tipos predefinidos
 * para la aplicacion de riego. Soporta envio thread-safe (con
 * deshabilitacion de interrupciones) y envio desde ISR.
 */

#ifndef MESSAGE_QUEUE_H
#define MESSAGE_QUEUE_H

#include <stdint.h>

/** Tamano del buffer circular (mensajes) */
#define QUEUE_SIZE 16

/**
 * @brief Tipos de mensajes del sistema de riego.
 * Cada tipo indica la semantica del mensaje para el consumidor.
 */
typedef enum {
    MSG_NONE = 0,           /**< Sin mensaje (placeholder) */
    MSG_SOIL_DRY,           /**< El suelo esta seco (necesita riego) */
    MSG_SOIL_WET,           /**< El suelo esta humedo (no regar) */
    MSG_MANUAL_TRIGGER,     /**< Riego manual solicitado por boton */
    MSG_LOG_TEXT,           /**< Texto para registrar en el log */
    MSG_DISPLAY_TEXT        /**< Texto para mostrar en el display */
} message_type_t;

/**
 * @brief Estructura de un mensaje en la cola.
 * Contiene tipo, dato numerico opcional, y texto.
 */
typedef struct {
    message_type_t type;    /**< Tipo de mensaje */
    uint32_t data;          /**< Dato numerico asociado (opcional) */
    char text[32];          /**< Texto del mensaje (para log/display) */
} message_t;

/**
 * @brief Estructura de la cola de mensajes (buffer circular).
 */
typedef struct {
    message_t buffer[QUEUE_SIZE];   /** Buffer circular de mensajes */
    int head;                       /** Indice de lectura */
    int tail;                       /** Indice de escritura */
    int count;                      /** Mensajes actualmente en cola */
} message_queue_t;

/** Inicializa la cola (head=tail=count=0) */
void mq_init(message_queue_t *q);

/** Envia un mensaje (thread-safe, deshabilita interrupciones) */
int mq_send(message_queue_t *q, message_t *msg);

/** Recibe un mensaje (thread-safe, no bloqueante) */
int mq_receive(message_queue_t *q, message_t *msg);

/** Envia un mensaje desde un ISR (no modifica estado de interrupciones) */
int mq_send_from_isr(message_queue_t *q, message_t *msg);

#endif
