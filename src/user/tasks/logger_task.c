#include <stdio.h>
#include <string.h>

#include "logger.h"
#include "log_memory.h"
#include "message_queue.h"
#include "user_app.h"
#include "syscalls.h"


/**
 * @brief Tarea de logger que se encarga de recibir mensajes de log desde otras tareas 
 * y escribirlos en el archivo de log usando el módulo de logger.
 */
void logger_task(void) {
    logger_init();
    message_t log_msg;

    log_memory_init();
    int flush_counter = 0;

    while (1) {
        sys_heartbeat();
        if (mq_receive(&log_queue, &log_msg) == 0) {
            if (log_msg.type == MSG_LOG_TEXT) {
                sys_sem_wait(&logger_sem);
                log_cache_write(log_msg.text);
                flush_counter++;
                //logger_write(log_msg.text);
                printf("[LOG %d] %s\n", flush_counter, log_msg.text);
                sys_sem_post(&logger_sem);
            }
        }
        /*
         * Flush periódico hacia Flash.
         *
         * Evita escribir Flash todo el tiempo.
         */
        if (flush_counter >= 10) {
            sys_sem_wait(&logger_sem);
            log_flush_all();
            printf("[LOG] Cache flushed to Flash\n");
            sys_sem_post(&logger_sem);
            flush_counter = 0;
        }
    }
}