#ifndef FLASH_QUEUE_H
#define FLASH_QUEUE_H

#define FLASH_QUEUE_SIZE 8
#define FLASH_ENTRY_LEN 64

typedef struct {
    char data[FLASH_ENTRY_LEN];
    int valid;
} flash_entry_t;

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
