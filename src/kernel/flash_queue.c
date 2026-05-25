#include "flash_queue.h"
#include "logger.h"
#include <string.h>
#include <stdio.h>

flash_queue_t flash_work_queue;

void flash_queue_init(void) {
    flash_work_queue.head = 0;
    flash_work_queue.tail = 0;
    flash_work_queue.count = 0;
}

int flash_queue_push(const char *data) {
    if (flash_work_queue.count >= FLASH_QUEUE_SIZE)
        return -1;
    strncpy(flash_work_queue.entries[flash_work_queue.tail].data, data, FLASH_ENTRY_LEN - 1);
    flash_work_queue.entries[flash_work_queue.tail].data[FLASH_ENTRY_LEN - 1] = '\0';
    flash_work_queue.tail = (flash_work_queue.tail + 1) % FLASH_QUEUE_SIZE;
    flash_work_queue.count++;
    return 0;
}

int flash_queue_pop(char *out) {
    if (flash_work_queue.count <= 0)
        return -1;
    strncpy(out, flash_work_queue.entries[flash_work_queue.head].data, FLASH_ENTRY_LEN);
    flash_work_queue.head = (flash_work_queue.head + 1) % FLASH_QUEUE_SIZE;
    flash_work_queue.count--;
    return 0;
}

void flash_queue_process(void) {
    char buf[FLASH_ENTRY_LEN];
    while (flash_queue_pop(buf) == 0) {
        logger_write(buf);
    }
}
