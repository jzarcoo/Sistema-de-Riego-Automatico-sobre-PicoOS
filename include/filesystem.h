#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include <stdint.h>
#include "hardware/flash.h"


// Constantes del FS
#define FS_BASE_OFFSET (1024 * 1024)  // Offset de 1MB (deja 1MB para el programa)
#define FS_BASE_ADDRESS (XIP_BASE + FS_BASE_OFFSET)
#define MAX_FILES 16
#define MAGIC_NUMBER 0x4649534F // "FISO"

// 8 bytes (cabecera) + 16 * 24 bytes (entradas) = 392 bytes totales
// Necesitamos exactamente 2 páginas (512 bytes) para programarlo por completo
#define META_PROGRAM_SIZE 512

// --- Tipos de Datos ---
typedef enum {
    STATUS_DELETED = 0x00,
    STATUS_OCCUPIED = 0xAA,
    STATUS_FREE     = 0xFF
} FileStatus;

typedef struct {
    char name[12];
    uint32_t offset;
    uint32_t size;
    uint8_t status;
    uint8_t padding[3];
} FileEntry;

typedef struct {
    uint32_t magic_number;
    uint32_t total_size;
    FileEntry entries[MAX_FILES];
} MetaData_Table;

// Declaración externa del mapa de metadatos mapeado en la Flash (XIP)
extern const MetaData_Table* fs_meta;

// --- API del Sistema de Archivos ---

/**
 * @brief Inicializa el sistema de archivos. Si no encuentra el Magic Number, formatea.
 */
void fs_init(void);

/**
 * @brief Formatea la región asignada al FS inicializando la tabla de metadatos.
 */
void fs_format(void);

/**
 * @brief Crea un archivo vacío reservando una entrada en la tabla de metadatos.
 * @return Índice de la entrada creada, -1 si está lleno, -2 si el nombre ya existe.
 */
int fs_create(const char* name);

/**
 * @brief Escribe un bloque completo de datos a un archivo mediante Read-Modify-Write.
 * @return 0 en éxito, -1 en caso de error o superposición de espacio.
 */
int fs_write(const char* name, const uint8_t* data, uint32_t size);

/**
 * @brief Lee los datos persistidos en Flash directamente usando direccionamiento XIP.
 * @return Cantidad de bytes leídos, o -1 si no se encuentra el archivo.
 */
int fs_read(const char* name, uint8_t* buffer, uint32_t max_size);

/**
 * @brief Realiza un borrado lógico del archivo cambiando su estado a STATUS_DELETED.
 * @return 0 en éxito, -1 si el archivo no existe.
 */
int fs_delete(const char* name);

/**
 * @brief Despliega el mapa de memoria actual y el estado de la tabla en consola.
 */
void fs_dump(void);

/**
 * @brief Compacta el sistema de archivos eliminando la fragmentación y los archivos borrados.
 */
void fs_compact(void);

/**
 * @brief Agrega datos al final de un archivo existente.
 * @return 0 en éxito, -1 si el archivo no existe.
 */
int fs_write_append(const char* name, const uint8_t* data, uint32_t size);

#endif // FILESYSTEM_H