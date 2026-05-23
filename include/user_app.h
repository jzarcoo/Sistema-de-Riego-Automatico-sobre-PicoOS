/**
 * @file user_app.h
 * @brief Recursos compartidos del espacio de usuario.
 *
 * Declara semaforos, colas de mensajes y prototipos de tareas.
 * Los recursos son creados e inicializados por el kernel boot.
 */

#ifndef USER_APP_H
#define USER_APP_H

#include "semaphore.h"
#include "message_queue.h"

/* Semaforos (creados por kernel, usados via syscall) */
extern kernel_semaphore_t irrigation_pump_sem;
extern kernel_semaphore_t logger_sem;
extern kernel_semaphore_t display_sem;

/* Colas de mensajes (creadas por kernel) */
extern message_queue_t irrigation_queue;
extern message_queue_t log_queue;
extern message_queue_t display_queue;

#endif
