#include "log_memory.h"
#include "logger.h"

#include <string.h>
#include <stdio.h>

/**
 * Un log_page_t es una página de log en la cache.
 */
typedef struct {
    char data[64]; // Mensaje
    int valid; // Si la página contiene un mensaje 
    int dirty; // Si la página ha sido modificada desde el último flush
    int last_use; // Contador para implementar LRU 
} log_page_t;

static log_page_t frames[LOG_FRAMES];

static int timer_counter = 0;

/**
 * @brief Busca un frame libre en la cache. Retorna el índice del frame o -1 si no hay ninguno.
 * @return Índice del frame libre o -1 si no hay ninguno.
 */
static int find_free_frame(void) {
    for (int i = 0; i < LOG_FRAMES; i++) {
        if (!frames[i].valid) {
            return i;
        }
    }
    return -1;
}

/**
 * @brief Reemplazo LRU.
 * @return Índice del frame víctima.
 */
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
        printf("[MEMORIA] Volcando frame %d -> Flash\n", victim);
    }
    return victim;
}

/**
 * @brief Inicializa la memoria de logs.
 */
void log_memory_init(void) {
    memset(frames, 0, sizeof(frames));
}

/** 
 * @brief Escribe un mensaje de log en la cache. 
 * Si no hay frames libres, se hace un flush de la página LRU.
 * @param msg Mensaje de log a escribir.
 */
void log_cache_write(const char* msg) {
    timer_counter++;
    int frame = find_free_frame();
    /*
     * Simulated page fault.
     */
    if (frame == -1) {
        printf("[PAGE FAULT] No hay frames libres.\n");
        frame = evict_lru();
    }
    strncpy(frames[frame].data, msg, 63);
    frames[frame].valid = 1;
    frames[frame].dirty = 1;
    frames[frame].last_use = timer_counter;
    printf("[CACHE] Escribiendo en frame %d: %s\n", frame, msg);
}

/**
 * @brief Hace flush de todas las páginas sucias a Flash.
 */
void log_flush_all(void) {
    for (int i = 0; i < LOG_FRAMES; i++) {
        if (frames[i].valid && frames[i].dirty) {
            logger_write(frames[i].data);
            frames[i].dirty = 0;
            printf("[FLUSH] Frame %d volcado a Flash.\n", i);
        }
    }
}