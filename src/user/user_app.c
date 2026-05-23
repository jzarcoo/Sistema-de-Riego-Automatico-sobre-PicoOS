/**
 * @file user_app.c
 * @brief Recursos compartidos del espacio de usuario.
 *
 * Define los semaforos y colas de mensajes globales que las tareas
 * usan para comunicarse. La inicializacion de estos recursos se
 * realiza en el kernel boot (main.c), no aqui.
 */

#include "user_app.h"

kernel_semaphore_t irrigation_pump_sem;
kernel_semaphore_t logger_sem;
kernel_semaphore_t display_sem;

message_queue_t irrigation_queue;
message_queue_t log_queue;
message_queue_t display_queue;
