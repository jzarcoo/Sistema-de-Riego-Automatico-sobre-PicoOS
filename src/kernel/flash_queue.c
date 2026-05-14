/**
 * @file flash_queue.c
 * @brief Cola de write-back diferido para operaciones de Flash.
 *
 * Las paginas dirty del page cache se encolan aqui en vez de
 * escribirse a Flash directamente (que causaria deadlock inter-core).
 * El idle loop del kernel drena esta cola en thread mode privilegiado.
 */

#include "flash_queue.h"
#include "logger.h"
#include <string.h>
#include <stdio.h>

flash_queue_t flash_work_queue;

/** @brief Inicializa la cola de flash (head=tail=count=0). */
void flash_queue_init(void) {
    flash_work_queue.head = 0;
    flash_work_queue.tail = 0;
    flash_work_queue.count = 0;
}

/** @brief Encola datos para escritura posterior a Flash. Retorna -1 si llena. */
int flash_queue_push(const char *data) {
    if (flash_work_queue.count >= FLASH_QUEUE_SIZE)
        return -1;
    strncpy(flash_work_queue.entries[flash_work_queue.tail].data, data, FLASH_ENTRY_LEN - 1);
    flash_work_queue.entries[flash_work_queue.tail].data[FLASH_ENTRY_LEN - 1] = '\0';
    flash_work_queue.tail = (flash_work_queue.tail + 1) % FLASH_QUEUE_SIZE;
    flash_work_queue.count++;
    return 0;
}

/** @brief Extrae un entry de la cola. Retorna -1 si vacia. */
int flash_queue_pop(char *out) {
    if (flash_work_queue.count <= 0)
        return -1;
    strncpy(out, flash_work_queue.entries[flash_work_queue.head].data, FLASH_ENTRY_LEN);
    flash_work_queue.head = (flash_work_queue.head + 1) % FLASH_QUEUE_SIZE;
    flash_work_queue.count--;
    return 0;
}

/** @brief Drena la cola completa escribiendo cada entry a Flash via logger_write. */
void flash_queue_process(void) {
    char buf[FLASH_ENTRY_LEN];
    while (flash_queue_pop(buf) == 0) {
        logger_write(buf);
    }
}
