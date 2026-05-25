/**
 * @file logger_task.c
 * @brief Tarea de logging con page cache y flush periodico a Flash.
 *
 * Consume mensajes MSG_LOG_TEXT de la cola log_queue y los almacena
 * en la page cache (log_memory). Cada 10 mensajes, ejecuta un flush
 * que vuelca las paginas dirty al filesystem en Flash. El acceso
 * esta protegido por logger_sem.
 *
 * Integra los conceptos de:
 * - Simulacion de memoria virtual (page cache LRU en log_memory.c)
 * - Sistema de archivos (PicoFS en filesystem.c)
 * - Productor-consumidor (colas de mensajes)
 *
 * Corre en Core 0 (plano de gestion).
 */

#include <string.h>

#include "message_queue.h"
#include "user_app.h"
#include "syscalls.h"

/**
 * @brief Loop principal de la tarea de logging.
 *
 * Inicializa el filesystem y la page cache. Luego consume mensajes
 * de log_queue, los escribe en la cache (simulando page faults con
 * LRU cuando se llena), y periodicamente vuelca todo a Flash.
 */
void logger_task(void) {
    sys_print("[CORE0] Logger Task iniciada.\n");
    sys_logger_init();
    message_t log_msg;
    int flush_counter = 0;

    while (1) {
        sys_heartbeat();
        if (mq_receive(&log_queue, &log_msg) == 0) {
            if (log_msg.type == MSG_LOG_TEXT) {
                sys_sem_wait(&logger_sem);
                sys_log_write(log_msg.text);
                flush_counter++;
                sys_sem_post(&logger_sem);
            }
        }
        if (flush_counter >= 50) {
            sys_sem_wait(&logger_sem);
            sys_log_flush();
            sys_sem_post(&logger_sem);
            flush_counter = 0;
        }
    }
}
