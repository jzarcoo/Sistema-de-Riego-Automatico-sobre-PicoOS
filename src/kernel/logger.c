/**
 * @file logger.c
 * @brief Modulo de logging persistente sobre Flash.
 *
 * Proporciona la capa de persistencia para el sistema de logs.
 * Utiliza el sistema de archivos PicoFS para almacenar eventos
 * de riego en un archivo plano en la memoria Flash del RP2040.
 * Se integra con el modulo log_memory (page cache LRU) que
 * invoca logger_write() cuando necesita volcar paginas sucias.
 */

#include <stdio.h>
#include <string.h>

#include "logger.h"
#include "filesystem.h"

/** Nombre del archivo de log en el filesystem PicoFS */
static const char* LOG_FILE_NAME = "irrig.log";

/**
 * @brief Inicializa el modulo de logging.
 *
 * Inicializa el sistema de archivos PicoFS y crea el archivo
 * de log si no existe. Si el archivo ya existe, la operacion
 * es idempotente.
 */
void logger_init(void) {
    fs_init();
    fs_create(LOG_FILE_NAME);
}

/**
 * @brief Escribe un evento de log en Flash.
 *
 * Agrega el texto del evento al final del archivo de log
 * usando fs_write_append (esquema read-modify-write).
 *
 * @param event Cadena de texto del evento a registrar (max 63 bytes).
 */
void logger_write(const char* event) {
    printf("[FLASH] %s\n", event);
    fs_write_append(LOG_FILE_NAME,
                    (const uint8_t*)event,
                    strlen(event));
}