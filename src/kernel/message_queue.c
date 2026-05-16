#include "message_queue.h"
#include "hardware/irq.h"
#include "hardware/sync.h"
/**
 * @brief Inicializa la cola de mensajes, estableciendo los i­ndices y el contador a cero.
 * @param q Puntero a la cola de mensajes a inicializar.
 */
void mq_init(message_queue_t *q) {
    q->head = 0;
    q->tail = 0;
    q->count = 0;
}

/** 
 * @brief Envi­a un mensaje a la cola. Si la cola esta llena, devuelve -1.
 * @param q Puntero a la cola de mensajes.
 * @param msg Puntero al mensaje a enviar.
 * @return 0 si el mensaje se envia correctamente, -1 si la cola esta llena.
 */
int mq_send(message_queue_t *q, message_t *msg) {
    uint32_t status = save_and_disable_interrupts();
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
 * @brief ISR-safe enqueue. Minimal, non-blocking, should be called from IRQ handlers.
 * Does not change interrupt state; returns -1 if queue is full.
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
 * @brief Recibe un mensaje de la cola. Si la cola estÃ¡ vacÃ­a, devuelve -1.
 * @param q Puntero a la cola de mensajes.
 * @param msg Puntero donde se almacenarÃ¡ el mensaje recibido.
 * @return 0 si el mensaje se recibiÃ³ correctamente, -1 si la cola estÃ¡ vacÃ­a.
 */
int mq_receive(message_queue_t *q, message_t *msg) {
    uint32_t status = save_and_disable_interrupts();
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

