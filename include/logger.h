/**
 * @file logger.h
 * @brief Interfaz del modulo de logging persistente sobre Flash.
 *
 * Provee funciones para inicializar el archivo de log en PicoFS
 * y escribir eventos que se persisten en la memoria Flash.
 */

#ifndef LOGGER_H
#define LOGGER_H

/** Inicializa el filesystem y crea el archivo de log */
void logger_init(void);

/** Escribe un evento al archivo de log en Flash (append) */
void logger_write(const char* event);

#endif
