/**
 * @file log_memory.h
 * @brief Interfaz de la page cache LRU para logs.
 *
 * Abstrae un buffer de paginas en RAM con reemplazo LRU.
 * Los logs se escriben primero en la cache y se vuelcan
 * a Flash periodicamente o cuando se necesita un frame.
 */

#ifndef LOG_MEMORY_H
#define LOG_MEMORY_H

/** Cantidad de frames en la page cache */
#define LOG_FRAMES 6

/** Inicializa todos los frames como invalidos */
void log_memory_init(void);

/** Escribe un mensaje en la cache (simula page fault si esta llena) */
void log_cache_write(const char* msg);

/** Vuelca todas las paginas dirty a Flash */
void log_flush_all(void);

#endif
