/**
 * @file flash_queue.h
 * @brief Cola de write-back diferido para persistencia a Flash.
 *
 * Buffer intermedio entre el page cache (que corre en SVC handler)
 * y las operaciones reales de Flash (que necesitan thread mode para
 * multicore_lockout). El idle loop del kernel drena esta cola.
 */

#ifndef FLASH_QUEUE_H
#define FLASH_QUEUE_H

#define FLASH_QUEUE_SIZE 8
#define FLASH_ENTRY_LEN 64

/** @brief Entry individual en la cola de flash. */
typedef struct {
    char data[FLASH_ENTRY_LEN];
    int valid;
} flash_entry_t;

/** @brief Cola circular de escrituras pendientes a Flash. */
typedef struct {
    flash_entry_t entries[FLASH_QUEUE_SIZE];
    int head;
    int tail;
    int count;
} flash_queue_t;

extern flash_queue_t flash_work_queue;

void flash_queue_init(void);
int flash_queue_push(const char *data);
int flash_queue_pop(char *out);
void flash_queue_process(void);

#endif
