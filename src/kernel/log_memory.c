#include "log_memory.h"
#include "logger.h"

#include <string.h>
#include <stdio.h>

typedef struct {
    char data[64];

    int valid;
    int dirty;

    int last_use;
} log_page_t;

static log_page_t frames[LOG_FRAMES];

static int timer_counter = 0;
// static int next_fifo = 0;

static int find_free_frame(void) {

    for (int i = 0; i < LOG_FRAMES; i++) {

        if (!frames[i].valid) {
            return i;
        }
    }

    return -1;
}

static int evict_lru(void) {

    int victim = 0;

    for (int i = 1; i < LOG_FRAMES; i++) {

        if (frames[i].last_use < frames[victim].last_use) {
            victim = i;
        }
    }

    /*
     * Flush to filesystem before eviction.
     */

    if (frames[victim].dirty) {

        logger_write(frames[victim].data);

        printf("[MEMORY] Flushed frame %d -> Flash\n", victim);
    }

    return victim;
}

void log_memory_init(void) {

    memset(frames, 0, sizeof(frames));
}

void log_cache_write(const char* msg) {

    timer_counter++;

    int frame = find_free_frame();

    /*
     * Simulated page fault.
     */
    if (frame == -1) {

        printf("[PAGE FAULT] No free frames.\n");

        frame = evict_lru();
    }

    strncpy(frames[frame].data, msg, 63);

    frames[frame].valid = 1;
    frames[frame].dirty = 1;
    frames[frame].last_use = timer_counter;

    printf("[CACHE] Stored log in frame %d\n", frame);
}

void log_flush_all(void) {

    for (int i = 0; i < LOG_FRAMES; i++) {

        if (frames[i].valid && frames[i].dirty) {

            logger_write(frames[i].data);

            frames[i].dirty = 0;

            printf("[FLUSH] Frame %d persisted.\n", i);
        }
    }
}