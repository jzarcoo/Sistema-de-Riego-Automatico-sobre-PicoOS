#include <stdio.h>

#include "logger.h"
#include "filesystem.h"

// Archivo de log para eventos de riego
static const char* LOG_FILE_NAME = "irrig.log";

/**
 * @brief Inicializa el módulo de logging creando el archivo de log en el sistema de archivos.
 * Si el archivo ya existe, no hace nada.
 */
void logger_init(void) {
    // fs_init();

    // if (fs_create(LOG_FILE_NAME) < 0) {
    //     // ya existe o error
    // }
}

/**
 * @brief Escribe un evento de log en el archivo de log. Usa fs_write_append para agregar al final del archivo.
 * @param event Cadena de texto que representa el evento a registrar.
 * Nota: Asegúrate de que el evento no exceda el tamaño máximo permitido por fs_write_append 
 */
void logger_write(const char* event) {
    printf("[FLASH] %s\n", event);
    // fs_write_append(LOG_FILE_NAME,
    //                 (const uint8_t*)event,
    //                 strlen(event));
}