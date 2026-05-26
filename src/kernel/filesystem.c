/**
 * @file filesystem.c
 * @brief Sistema de archivos plano (PicoFS) sobre memoria Flash del RP2040.
 *
 * Implementa un filesystem minimalista con tabla de metadatos en el
 * primer sector de Flash y datos contiguos en sectores subsiguientes.
 * Operaciones soportadas: format, init, create, write, read, delete,
 * append, compact y dump.
 *
 * Restricciones de Flash:
 * - Solo se puede borrar por sectores completos (4096 bytes).
 * - Solo se puede programar por paginas (256 bytes).
 * - Escritura requiere read-modify-write para preservar datos existentes.
 *
 * Seguridad multicore: Todas las operaciones Flash usan flash_safe_begin/end
 * que pausan el otro core (multicore_lockout) y deshabilitan interrupciones.
 */

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/sync.h"

#include "filesystem.h"

/** Puntero XIP a la tabla de metadatos en Flash (lectura directa) */
const MetaData_Table* fs_meta = (const MetaData_Table*)(FS_BASE_ADDRESS);

/** Almacena el estado de interrupciones para restaurar despues de Flash ops */
static uint32_t flash_saved_ints;

/**
 * @brief Prepara el sistema para una operacion de Flash segura.
 *
 * Pausa el otro core (multicore_lockout) y deshabilita interrupciones.
 * Necesario porque durante flash_range_erase/program, el XIP se
 * desactiva y ningun codigo puede ejecutar desde Flash.
 * Usa timeout de 50ms para evitar deadlock si Core 1 esta en SVC.
 */
static void flash_safe_begin(void) {
    multicore_lockout_start_timeout_us(50000);
    flash_saved_ints = save_and_disable_interrupts();
}

/**
 * @brief Finaliza la operacion de Flash segura.
 * Restaura interrupciones y libera el otro core.
 */
static void flash_safe_end(void) {
    restore_interrupts(flash_saved_ints);
    multicore_lockout_end_blocking();
}

/**
 * @brief Formatea el filesystem (borra todos los archivos).
 *
 * Escribe un nuevo MetaData_Table con magic_number valido y
 * todas las entradas en STATUS_FREE. Borra el sector de metadatos.
 */
void fs_format() {
    MetaData_Table new_meta;
    memset(&new_meta, 0xFF, sizeof(MetaData_Table));
    new_meta.magic_number = MAGIC_NUMBER;
    new_meta.total_size = 0;

    uint8_t meta_buf[META_PROGRAM_SIZE];
    memset(meta_buf, 0xFF, META_PROGRAM_SIZE);
    memcpy(meta_buf, &new_meta, sizeof(MetaData_Table));

    flash_safe_begin();
    flash_range_erase(FS_BASE_OFFSET, FLASH_SECTOR_SIZE);
    flash_range_program(FS_BASE_OFFSET, meta_buf, META_PROGRAM_SIZE);
    flash_safe_end();

    printf("FS: Formateado exitosamente.\n");
}

/**
 * @brief Inicializa el filesystem.
 *
 * Verifica el magic_number en la tabla de metadatos. Si no coincide
 * (Flash virgen o corrupta), ejecuta un formateo automatico.
 */
void fs_init() {
    if (fs_meta->magic_number != MAGIC_NUMBER) {
        printf("FS Warning: Magic Number no encontrado. Forzando formateo...\n");
        fs_format();
        return;
    }
    printf("FS Inicializado exitosamente. Magic Number: 0x%08X\n", fs_meta->magic_number);
}

/**
 * @brief Crea un archivo nuevo en el filesystem.
 *
 * Busca una entrada libre en la tabla de metadatos, calcula el offset
 * contiguo despues del ultimo archivo, y registra el nuevo archivo.
 *
 * @param name Nombre del archivo (maximo 11 caracteres + null).
 * @return Indice de la entrada creada, -1 si no hay espacio, -2 si ya existe.
 */
int fs_create(const char* name) {
    if (strlen(name) >= 12) {
        printf("FS Error: Nombre muy largo.\n");
        return -1;
    }
    int free_index = -1, next_free_offset = FS_BASE_OFFSET + FLASH_SECTOR_SIZE;
    for (int i = 0; i < MAX_FILES; i++) {
        if (fs_meta->entries[i].status == STATUS_FREE && free_index == -1) {
            free_index = i;
        }
        if (fs_meta->entries[i].status != STATUS_OCCUPIED) {
            continue;
        }
        if (strncmp(fs_meta->entries[i].name, name, 12) == 0) {
            printf("FS: '%s' ya existe, reutilizando.\n", name);
            return -2;
        }
        int offset = fs_meta->entries[i].offset, size = fs_meta->entries[i].size;
        int sectors_needed = (size == 0) ? 1 : ((size + FLASH_SECTOR_SIZE - 1) / FLASH_SECTOR_SIZE);
        int free_offset = offset + sectors_needed * FLASH_SECTOR_SIZE;
        if (free_offset > next_free_offset) {
            next_free_offset = free_offset;
        }
    }
    if (free_index == -1) {
        printf("FS Error: No hay espacio para mas archivos.\n");
        return -1;
    }

    MetaData_Table updated_meta;
    memcpy(&updated_meta, fs_meta, sizeof(MetaData_Table));

    strncpy(updated_meta.entries[free_index].name, name, 12);
    updated_meta.entries[free_index].offset = next_free_offset;
    updated_meta.entries[free_index].size = 0;
    updated_meta.entries[free_index].status = STATUS_OCCUPIED;

    uint8_t meta_buf[META_PROGRAM_SIZE];
    memset(meta_buf, 0xFF, META_PROGRAM_SIZE);
    memcpy(meta_buf, &updated_meta, sizeof(MetaData_Table));

    flash_safe_begin();
    flash_range_erase(FS_BASE_OFFSET, FLASH_SECTOR_SIZE);
    flash_range_program(FS_BASE_OFFSET, meta_buf, META_PROGRAM_SIZE);
    flash_safe_end();

    return free_index;
}

/**
 * @brief Escribe datos en un archivo existente (sobreescribe).
 *
 * Realiza read-modify-write: borra los sectores necesarios y programa
 * los datos por paginas de 256 bytes. Actualiza el tamano en metadatos.
 *
 * @param name Nombre del archivo destino.
 * @param data Puntero a los datos a escribir.
 * @param size Tamano en bytes de los datos.
 * @return 0 si exitoso, -1 si el archivo no existe o no hay espacio.
 */
int fs_write(const char* name, const uint8_t* data, uint32_t size) {
    int target_idx = -1;

    for (int i = 0; i < MAX_FILES; i++) {
        if (fs_meta->entries[i].status == STATUS_OCCUPIED &&
            strncmp(fs_meta->entries[i].name, name, 12) == 0) {
            target_idx = i;
            break;
        }
    }

    if (target_idx == -1) {
        printf("FS Error: Archivo '%s' no encontrado.\n", name);
        return -1;
    }

    uint32_t offset = fs_meta->entries[target_idx].offset;
    int sectors_needed = (size + FLASH_SECTOR_SIZE - 1) / FLASH_SECTOR_SIZE;
    int total_size = sectors_needed * FLASH_SECTOR_SIZE;

    int max_offset = FS_BASE_OFFSET + FLASH_SECTOR_SIZE * MAX_FILES;
    if (offset + total_size > max_offset) {
        printf("FS Error: No hay suficiente espacio contiguo para escribir '%s'.\n", name);
        return -1;
    }
    for(int i = 0; i < MAX_FILES; i++) {
        if (i != target_idx && fs_meta->entries[i].status == STATUS_OCCUPIED) {
            int cur_offset = fs_meta->entries[i].offset;
            int cur_size = fs_meta->entries[i].size;

            int cur_sectors = (cur_size + FLASH_SECTOR_SIZE - 1) / FLASH_SECTOR_SIZE;
            int cur_total_size = cur_sectors * FLASH_SECTOR_SIZE;

            if ((offset < cur_total_size + cur_offset) && (offset + total_size > cur_offset)) {
                printf("FS Error: El espacio requerido para '%s' se superpone con '%s'.\n", name, fs_meta->entries[i].name);
                return -1;
            }
        }
    }

    uint8_t page_buf[FLASH_PAGE_SIZE];

    flash_safe_begin();
    flash_range_erase(offset, total_size);
    for (int i = 0; i < size; i += FLASH_PAGE_SIZE) {
        memset(page_buf, 0xFF, FLASH_PAGE_SIZE);
        int block_size = size - i > FLASH_PAGE_SIZE ? FLASH_PAGE_SIZE : (size - i);
        memcpy(page_buf, data + i, block_size);
        flash_range_program(offset + i, page_buf, FLASH_PAGE_SIZE);
    }
    flash_safe_end();

    if (size == fs_meta->entries[target_idx].size) {
        return 0;
    }

    MetaData_Table updated_meta;
    memcpy(&updated_meta, fs_meta, sizeof(MetaData_Table));
    updated_meta.entries[target_idx].size = size;

    uint8_t meta_buf[META_PROGRAM_SIZE];
    memset(meta_buf, 0xFF, META_PROGRAM_SIZE);
    memcpy(meta_buf, &updated_meta, sizeof(MetaData_Table));

    flash_safe_begin();
    flash_range_erase(FS_BASE_OFFSET, FLASH_SECTOR_SIZE);
    flash_range_program(FS_BASE_OFFSET, meta_buf, META_PROGRAM_SIZE);
    flash_safe_end();

    return 0;
}

/**
 * @brief Lee el contenido de un archivo.
 *
 * Usa lectura XIP (Execute-In-Place): la Flash esta mapeada en el
 * espacio de direcciones a partir de XIP_BASE, asi que se puede
 * leer directamente con memcpy sin operaciones especiales.
 *
 * @param name Nombre del archivo a leer.
 * @param buffer Buffer destino donde se copiaran los datos.
 * @param max_size Tamano maximo a leer (bytes).
 * @return Cantidad de bytes leidos, o -1 si el archivo no existe.
 */
int fs_read(const char* name, uint8_t* buffer, uint32_t max_size) {
    int target_idx = -1;
    for (int i = 0; i < MAX_FILES; i++) {
        if (fs_meta->entries[i].status == STATUS_OCCUPIED &&
            strncmp(fs_meta->entries[i].name, name, 12) == 0) {
            target_idx = i;
            break;
        }
    }
    if (target_idx == -1) {
        printf("FS Error: Archivo '%s' no encontrado.\n", name);
        return -1;
    }
    uint32_t file_size = fs_meta->entries[target_idx].size;
    uint32_t to_read = (file_size < max_size) ? file_size : max_size;
    uint32_t address = XIP_BASE + fs_meta->entries[target_idx].offset;
    memcpy(buffer, (const void*)address, to_read);
    return to_read;
}

/**
 * @brief Elimina un archivo (borrado logico).
 *
 * Marca la entrada como STATUS_DELETED en la tabla de metadatos.
 * Los datos permanecen en Flash hasta que se ejecute fs_compact().
 *
 * @param name Nombre del archivo a borrar.
 * @return 0 si exitoso, -1 si el archivo no existe.
 */
int fs_delete(const char* name) {
    int target_idx = -1;
    for (int i = 0; i < MAX_FILES; i++) {
        if (fs_meta->entries[i].status == STATUS_OCCUPIED &&
            strncmp(fs_meta->entries[i].name, name, 12) == 0) {
            target_idx = i;
            break;
        }
    }
    if (target_idx == -1) {
        printf("FS Error: Archivo '%s' no encontrado para borrar.\n", name);
        return -1;
    }
    MetaData_Table updated_meta;
    memcpy(&updated_meta, fs_meta, sizeof(MetaData_Table));
    updated_meta.entries[target_idx].status = STATUS_DELETED;

    uint8_t meta_buf[META_PROGRAM_SIZE];
    memset(meta_buf, 0xFF, META_PROGRAM_SIZE);
    memcpy(meta_buf, &updated_meta, sizeof(MetaData_Table));

    flash_safe_begin();
    flash_range_erase(FS_BASE_OFFSET, FLASH_SECTOR_SIZE);
    flash_range_program(FS_BASE_OFFSET, meta_buf, META_PROGRAM_SIZE);
    flash_safe_end();

    return 0;
}

/**
 * @brief Muestra el mapa del filesystem por consola serial.
 * Lista todas las entradas no libres con nombre, offset, tamano y estado.
 */
void fs_dump() {
    printf("\n=== Mapa de PicoFS ===\n");
    printf("Magic Number: 0x%08X\n", fs_meta->magic_number);
    printf("%-12s  %-10s | %-8s | %-8s\n", "Name", "Offset", "Size", "Status");
    printf("------------------------------------------------\n");

    for (int i = 0; i < MAX_FILES; i++) {
        if (fs_meta->entries[i].status != STATUS_FREE) {
            const char* status_str = (fs_meta->entries[i].status == STATUS_OCCUPIED) ? "OCCUPIED" :
                                        (fs_meta->entries[i].status == STATUS_DELETED) ? "DELETED" : "UNKNOWN";
            printf("%-12s  0x%08X | %-8u | %-8s\n", fs_meta->entries[i].name, fs_meta->entries[i].offset, fs_meta->entries[i].size, status_str);
        }
    }

    printf("==============================\n\n");
}

/**
 * @brief Compacta el filesystem eliminando huecos de archivos borrados.
 *
 * Reubica archivos validos de forma contigua, recalculando offsets
 * y moviendo datos en Flash cuando es necesario. Limpia entradas
 * con STATUS_DELETED.
 */
void fs_compact(){
    MetaData_Table updated_meta;
    memcpy(&updated_meta, fs_meta, sizeof(MetaData_Table));

    int order[MAX_FILES];
    int count = 0;
    for (int i = 0; i < MAX_FILES; i++) {
        if (fs_meta->entries[i].status == STATUS_OCCUPIED) {
            order[count++] = i;
        } else {
            updated_meta.entries[i].status = STATUS_FREE;
            updated_meta.entries[i].offset = 0;
            updated_meta.entries[i].size = 0;
            memset(updated_meta.entries[i].name, 0, 12);
        }
    }

    flash_safe_begin();

    for (int i = 0; i < count; i++) {
        int idx = order[i];
        if (i == 0) {
            updated_meta.entries[idx].offset = FS_BASE_OFFSET + FLASH_SECTOR_SIZE;
        } else {
            int prev_idx = order[i-1];
            int prev_size = updated_meta.entries[prev_idx].size;
            int prev_sectors = (prev_size == 0) ? 1 : ((prev_size + FLASH_SECTOR_SIZE - 1) / FLASH_SECTOR_SIZE);
            updated_meta.entries[idx].offset = updated_meta.entries[prev_idx].offset + prev_sectors * FLASH_SECTOR_SIZE;
            if (updated_meta.entries[idx].offset != fs_meta->entries[idx].offset) {
                uint8_t page_buf[FLASH_PAGE_SIZE];
                int file_size = updated_meta.entries[idx].size;
                const uint8_t* data = (const uint8_t*)(XIP_BASE + fs_meta->entries[idx].offset);
                int sectors_needed = (file_size == 0) ? 1 : ((file_size + FLASH_SECTOR_SIZE - 1) / FLASH_SECTOR_SIZE);
                int total_size = sectors_needed * FLASH_SECTOR_SIZE;

                flash_range_erase(updated_meta.entries[idx].offset, total_size);

                for (int j = 0; j < file_size; j += FLASH_PAGE_SIZE) {
                    memset(page_buf, 0xFF, FLASH_PAGE_SIZE);
                    int block_size = file_size - j > FLASH_PAGE_SIZE ? FLASH_PAGE_SIZE : (file_size - j);
                    memcpy(page_buf, data + j, block_size);
                    flash_range_program(updated_meta.entries[idx].offset + j, page_buf, FLASH_PAGE_SIZE);
                }
            }
        }
    }

    uint8_t meta_buf[META_PROGRAM_SIZE];
    memset(meta_buf, 0xFF, META_PROGRAM_SIZE);
    memcpy(meta_buf, &updated_meta, sizeof(MetaData_Table));

    flash_range_erase(FS_BASE_OFFSET, FLASH_SECTOR_SIZE);
    flash_range_program(FS_BASE_OFFSET, meta_buf, META_PROGRAM_SIZE);

    flash_safe_end();
}

/**
 * @brief Agrega datos al final de un archivo existente (append).
 *
 * Implementa read-modify-write: lee el contenido actual del archivo,
 * concatena los datos nuevos, borra los sectores necesarios,
 * y reprograma todo el contenido. Actualiza tamano en metadatos.
 *
 * @param name Nombre del archivo a modificar.
 * @param data Puntero a los datos a agregar al final.
 * @param size Tamano en bytes de los datos nuevos.
 * @return 0 si exitoso, -1 si el archivo no existe.
 */
int fs_write_append(const char* name,
                    const uint8_t* data,
                    uint32_t size)
{
    int target_idx = -1;

    for (int i = 0; i < MAX_FILES; i++) {

        if (fs_meta->entries[i].status == STATUS_OCCUPIED &&
            strncmp(fs_meta->entries[i].name, name, 12) == 0) {

            target_idx = i;
            break;
        }
    }

    if (target_idx == -1) {
        return -1;
    }

    uint32_t current_size =
        fs_meta->entries[target_idx].size;

    uint32_t write_offset =
        fs_meta->entries[target_idx].offset + current_size;

    uint32_t new_size =
        current_size + size;

    uint32_t sectors_needed =
        (new_size + FLASH_SECTOR_SIZE - 1)
        / FLASH_SECTOR_SIZE;

    uint32_t total_size =
        sectors_needed * FLASH_SECTOR_SIZE;

    uint8_t temp_buffer[total_size];

    memset(temp_buffer, 0xFF, total_size);

    memcpy(temp_buffer,
           (const void*)(XIP_BASE +
           fs_meta->entries[target_idx].offset),
           current_size);

    memcpy(temp_buffer + current_size,
           data,
           size);

    uint8_t page_buf[FLASH_PAGE_SIZE];

    flash_safe_begin();

    flash_range_erase(fs_meta->entries[target_idx].offset, total_size);

    for (uint32_t i = 0; i < total_size; i += FLASH_PAGE_SIZE) {
        memcpy(page_buf, temp_buffer + i, FLASH_PAGE_SIZE);
        flash_range_program(fs_meta->entries[target_idx].offset + i, page_buf, FLASH_PAGE_SIZE);
    }

    flash_safe_end();

    MetaData_Table updated_meta;
    memcpy(&updated_meta, fs_meta, sizeof(MetaData_Table));
    updated_meta.entries[target_idx].size = new_size;

    uint8_t meta_buf[META_PROGRAM_SIZE];
    memset(meta_buf, 0xFF, META_PROGRAM_SIZE);
    memcpy(meta_buf, &updated_meta, sizeof(MetaData_Table));

    flash_safe_begin();
    flash_range_erase(FS_BASE_OFFSET, FLASH_SECTOR_SIZE);
    flash_range_program(FS_BASE_OFFSET, meta_buf, META_PROGRAM_SIZE);
    flash_safe_end();

    return 0;
}
