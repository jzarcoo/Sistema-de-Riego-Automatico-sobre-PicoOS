#ifndef MESSAGE_QUEUE_H
#define MESSAGE_QUEUE_H

#include <stdint.h>

#define QUEUE_SIZE 16

// Definición de tipos de mensajes para la aplicación de riego
typedef enum {
    MSG_NONE = 0,
    // Mensaje indicando que el suelo está seco
    MSG_SOIL_DRY,
    // Mensaje indicando que el suelo está húmedo
    MSG_SOIL_WET,
    // Mensaje para activar el riego manualmente
    MSG_MANUAL_TRIGGER,
    // Mensaje de log para eventos importantes
    MSG_LOG_TEXT,
    // Mensaje para actualizar el display OLED
    MSG_DISPLAY_TEXT
} message_type_t;

// Estructura de un mensaje en la cola
typedef struct {
    // Tipo de mensaje
    message_type_t type;
    // Datos asociados al mensaje
    uint32_t data;
    // Texto opcional para mensajes de log o display 
    char text[32];
} message_t;

// Estructura de la cola de mensajes
typedef struct {
    message_t buffer[QUEUE_SIZE]; // Buffer circular para almacenar los mensajes
    int head;  // Índice de la cabeza de la cola
    int tail;  // Índice de la cola
    int count; // Número de mensajes actualmente en la cola
} message_queue_t;

// Prototipos de funciones para la cola de mensajes
void mq_init(message_queue_t *q);

int mq_send(message_queue_t *q, message_t *msg);

int mq_receive(message_queue_t *q, message_t *msg);

#endif // MESSAGE_QUEUE_H