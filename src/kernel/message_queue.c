/**
 * @file message_queue.c
 * @brief Colas de mensajes para comunicacion entre tareas (IPC).
 *
 * Implementa un buffer circular thread-safe mediante deshabilitacion
 * de interrupciones. Soporta envio desde contexto normal (mq_send)
 * y desde ISR (mq_send_from_isr). Las tareas productoras envian
 * mensajes tipados (MSG_SOIL_DRY, MSG_LOG_TEXT, etc.) y las tareas
 * consumidoras los extraen con mq_receive. Este esquema productor-
 * consumidor desacopla las tareas y evita competencia por recursos.
 */

#include <stdio.h>
#include "message_queue.h"
#include "hardware/irq.h"
#include "hardware/sync.h"

static int mq_check(message_queue_t *q, const char *ctx) {
    if (q->count < 0 || q->count > QUEUE_SIZE ||
        q->head < 0 || q->head >= QUEUE_SIZE ||
        q->tail < 0 || q->tail >= QUEUE_SIZE) {
        printf("[MQ CORRUPT] %s count=%d head=%d tail=%d\n",
               ctx, q->count, q->head, q->tail);
        return 1;
    }
    return 0;
}

/**
 * @brief Inicializa la cola de mensajes.
 *
 * Establece head, tail y count a cero. Debe llamarse antes
 * de cualquier operacion send/receive.
 *
 * @param q Puntero a la cola de mensajes a inicializar.
 */
void mq_init(message_queue_t *q) {
    q->head = 0;
    q->tail = 0;
    q->count = 0;
}

/**
 * @brief Envia un mensaje a la cola (contexto normal).
 *
 * Deshabilita interrupciones para garantizar atomicidad.
 * Si la cola esta llena, retorna -1 sin bloquear.
 *
 * @param q Puntero a la cola de mensajes.
 * @param msg Puntero al mensaje a enviar (se copia al buffer).
 * @return 0 si el mensaje se envio correctamente, -1 si la cola esta llena.
 */
int mq_send(message_queue_t *q, message_t *msg) {
    uint32_t status = save_and_disable_interrupts();
    if (mq_check(q, "send")) { restore_interrupts(status); return -1; }
    if (q->count >= QUEUE_SIZE) {
        restore_interrupts(status);
        return -1;
    }
    q->buffer[q->tail] = *msg;
    q->tail = (q->tail + 1) % QUEUE_SIZE;
    q->count++;
    restore_interrupts(status);
    return 0;
}

/**
 * @brief Envia un mensaje desde un handler de interrupcion (ISR-safe).
 *
 * No modifica el estado de interrupciones (ya estamos en ISR).
 * No bloquea; retorna -1 si la cola esta llena.
 *
 * @param q Puntero a la cola de mensajes.
 * @param msg Puntero al mensaje a enviar.
 * @return 0 si exitoso, -1 si la cola esta llena.
 */
int mq_send_from_isr(message_queue_t *q, message_t *msg) {
    if (q->count >= QUEUE_SIZE) {
        return -1;
    }
    q->buffer[q->tail] = *msg;
    q->tail = (q->tail + 1) % QUEUE_SIZE;
    q->count++;
    return 0;
}

/**
 * @brief Recibe un mensaje de la cola.
 *
 * Deshabilita interrupciones para garantizar atomicidad.
 * Si la cola esta vacia, retorna -1 sin bloquear.
 *
 * @param q Puntero a la cola de mensajes.
 * @param msg Puntero donde se almacenara el mensaje recibido.
 * @return 0 si el mensaje se recibio correctamente, -1 si la cola esta vacia.
 */
int mq_receive(message_queue_t *q, message_t *msg) {
    uint32_t status = save_and_disable_interrupts();
    if (mq_check(q, "recv")) { restore_interrupts(status); return -1; }
    if (q->count <= 0) {
        restore_interrupts(status);
        return -1;
    }
    *msg = q->buffer[q->head];
    q->head = (q->head + 1) % QUEUE_SIZE;
    q->count--;
    restore_interrupts(status);
    return 0;
}
