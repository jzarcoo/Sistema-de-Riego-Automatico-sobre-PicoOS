#include "message_queue.h"

/**
 * @brief Inicializa la cola de mensajes, estableciendo los índices y el contador a cero.
 * @param q Puntero a la cola de mensajes a inicializar.
 */
void mq_init(message_queue_t *q) {
    q->head = 0;
    q->tail = 0;
    q->count = 0;
}

/** 
 * @brief Envía un mensaje a la cola. Si la cola está llena, devuelve -1.
 * @param q Puntero a la cola de mensajes.
 * @param msg Puntero al mensaje a enviar.
 * @return 0 si el mensaje se envió correctamente, -1 si la cola está llena.
 */
int mq_send(message_queue_t *q, message_t *msg) {
    if (q->count >= QUEUE_SIZE) {
        return -1;
    }
    q->buffer[q->tail] = *msg;
    q->tail = (q->tail + 1) % QUEUE_SIZE;
    q->count++;
    return 0;
}

/**
 * @brief Recibe un mensaje de la cola. Si la cola está vacía, devuelve -1.
 * @param q Puntero a la cola de mensajes.
 * @param msg Puntero donde se almacenará el mensaje recibido.
 * @return 0 si el mensaje se recibió correctamente, -1 si la cola está vacía.
 */
int mq_receive(message_queue_t *q, message_t *msg) {
    if (q->count <= 0) {
        return -1;
    }
    *msg = q->buffer[q->head];
    q->head = (q->head + 1) % QUEUE_SIZE;
    q->count--;
    return 0;
}