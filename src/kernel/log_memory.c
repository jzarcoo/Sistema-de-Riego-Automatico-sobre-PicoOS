/**
 * @file log_memory.c
 * @brief Simulacion de memoria virtual con page cache y reemplazo LRU.
 *
 * Implementa un buffer de paginas en RAM que actua como cache entre
 * las tareas productoras de logs y el almacenamiento persistente en Flash.
 * Cuando no hay frames libres, simula un "page fault" y aplica el
 * algoritmo LRU (Least Recently Used) para seleccionar la victima,
 * volcando su contenido a Flash antes de reutilizar el frame.
 *
 * Flujo:
 * 1. log_cache_write() busca un frame libre.
 * 2. Si no hay (page fault), evict_lru() encuentra la pagina menos
 *    usada recientemente, la vuelca a Flash si esta dirty, y libera el frame.
 * 3. log_flush_all() vuelca todas las paginas dirty (flush periodico).
 */

#include "log_memory.h"
#include "flash_queue.h"

#include <string.h>
#include <stdio.h>

/**
 * @brief Estructura de una pagina en la cache de logs.
 *
 * Cada frame almacena un mensaje de log con metadatos para
 * controlar validez, modificacion y politica de reemplazo.
 */
typedef struct {
    char data[64];  /**< Contenido del mensaje de log */
    int valid;      /**< 1 si el frame contiene datos validos */
    int dirty;      /**< 1 si fue modificado desde el ultimo flush */
    int last_use;   /**< Contador temporal para politica LRU */
} log_page_t;

/** Pool de frames de la page cache */
static log_page_t frames[LOG_FRAMES];

/** Contador global de accesos (reloj logico para LRU) */
static int timer_counter = 0;

/**
 * @brief Busca un frame libre (invalido) en la cache.
 * @return Indice del frame libre, o -1 si todos estan ocupados.
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
 * @brief Selecciona la pagina victima usando politica LRU.
 *
 * Recorre todos los frames y selecciona el que tiene el menor
 * valor de last_use (menos recientemente usado). Si la pagina
 * esta dirty, la vuelca a Flash antes de liberarla.
 *
 * @return Indice del frame victima (listo para reutilizar).
 */
static int evict_lru(void) {
    int victim = 0;
    for (int i = 1; i < LOG_FRAMES; i++) {
        if (frames[i].last_use < frames[victim].last_use) {
            victim = i;
        }
    }
    if (frames[victim].dirty) {
        flash_queue_push(frames[victim].data);
        printf("[EVICT] frame %d -> flash queue\n", victim);
    }
    return victim;
}

/**
 * @brief Inicializa la page cache de logs.
 * Pone todos los frames en estado invalido (vacios).
 */
void log_memory_init(void) {
    memset(frames, 0, sizeof(frames));
}

/**
 * @brief Escribe un mensaje de log en la cache.
 *
 * Busca un frame libre; si no hay, simula un page fault e invoca
 * el algoritmo de reemplazo LRU. Marca el frame como valid y dirty.
 *
 * @param msg Mensaje de log a almacenar (maximo 63 caracteres).
 */
void log_cache_write(const char* msg) {
    timer_counter++;
    int frame = find_free_frame();
    if (frame == -1) {
        printf("[Cache evict] No hay frames libres.\n");
        frame = evict_lru();
    }
    strncpy(frames[frame].data, msg, 63);
    frames[frame].valid = 1;
    frames[frame].dirty = 1;
    frames[frame].last_use = timer_counter;
    printf("[CACHE] Escribiendo en frame %d: %s\n", frame, msg);
}

/**
 * @brief Vuelca todas las paginas dirty a Flash (flush completo).
 *
 * Recorre todos los frames validos con dirty=1, los escribe en
 * el filesystem via logger_write(), y limpia el flag dirty.
 */
void log_flush_all(void) {
    for (int i = 0; i < LOG_FRAMES; i++) {
        if (frames[i].valid && frames[i].dirty) {
            flash_queue_push(frames[i].data);
            frames[i].dirty = 0;
            printf("[FLUSH] Frame %d -> flash queue\n", i);
        }
    }
}
