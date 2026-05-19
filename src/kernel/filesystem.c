#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/sync.h"

#include "filesystem.h"

// Puntero para leer la memoria Flash directamente mapeada (XIP - Execute In Place)
const MetaData_Table* fs_meta = (const MetaData_Table*)(FS_BASE_ADDRESS);

static uint32_t flash_saved_ints;

static void flash_safe_begin(void) {
    multicore_lockout_start_blocking();
    flash_saved_ints = save_and_disable_interrupts();
}

static void flash_safe_end(void) {
    restore_interrupts(flash_saved_ints);
    multicore_lockout_end_blocking();
}

// --- MÓDULO 1: Inicialización y Superbloque ---

// fs_format() se entrega resuelta como ejemplo de uso de la API de Flash
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

void fs_init() {
    if (fs_meta->magic_number != MAGIC_NUMBER) {
        printf("FS Warning: Magic Number no encontrado. Forzando formateo...\n");
        fs_format();
        return;
    } 
    printf("FS Inicializado exitosamente. Magic Number: 0x%08X\n", fs_meta->magic_number);
}

// --- MÓDULO 2: Asignación de Metadatos y Alineación ---

int fs_create(const char* name) {
    if (strlen(name) >= 12) {
        printf("FS Error: Nombre muy largo.\n");
        return -1;
    }
    // CHALLENGE 2 - Creación de archivo
    // 1. Itera sobre fs_meta->entries buscando la primera entrada con status == STATUS_FREE.
    // 2. Durante la iteración, verifica que no exista ya un archivo con el mismo nombre y status == STATUS_OCCUPIED.
    //    Si existe, retorna error (-2).
    // 3. Calcula la variable 'next_free_offset'. Para un FS contiguo, es el offset del último archivo
    //    redondeado hacia arriba al siguiente FLASH_SECTOR_SIZE (4096).
    // 4. Copia los metadatos actuales a una variable local: MetaData_Table updated_meta;
    // 5. En 'updated_meta', asigna a la entrada libre encontrada: el nombre, el offset calculado,
    //    tamaño 0, y cambia su estado a STATUS_OCCUPIED.
    // 6. Escribe 'updated_meta' en la Flash (revisa fs_format para ver cómo borrar y programar).
    // 7. Retorna el índice (free_index) creado.
    int free_index = -1, next_free_offset = FS_BASE_OFFSET + FLASH_SECTOR_SIZE; 
    for (int i = 0; i < MAX_FILES; i++) {
        if (fs_meta->entries[i].status == STATUS_FREE && free_index == -1) {
            free_index = i;
        }
        if (fs_meta->entries[i].status != STATUS_OCCUPIED) {
            continue;
        }
        if (strncmp(fs_meta->entries[i].name, name, 12) == 0) {
            printf("FS Error: Archivo con nombre '%s' ya existe.\n", name);
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
        printf("FS Error: No hay espacio para más archivos.\n");
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

// --- MÓDULO 3: Read-Modify-Write ---

int fs_write(const char* name, const uint8_t* data, uint32_t size) {
    int target_idx = -1;

    // Búsqueda del archivo
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

    //  Escritura Flash y Actualización de Metadatos
    // 1. Recupera el offset asignado para este archivo desde los metadatos.
    uint32_t offset = fs_meta->entries[target_idx].offset;
    // 2. Calcula cuántos sectores requiere 'size' bytes. Recuerda que solo puedes borrar en
    //    múltiplos de FLASH_SECTOR_SIZE (4096).
    int sectors_needed = (size + FLASH_SECTOR_SIZE - 1) / FLASH_SECTOR_SIZE;
    int total_size = sectors_needed * FLASH_SECTOR_SIZE;

    // extra: prevención de desbordamiento
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

int fs_read(const char* name, uint8_t* buffer, uint32_t max_size) {
    //  Lectura XIP (Execute-In-Place)
    // 1. Busca el índice del archivo por su nombre (igual que en fs_write).
    int target_idx = -1;
    for (int i = 0; i < MAX_FILES; i++) {
        if (fs_meta->entries[i].status == STATUS_OCCUPIED &&
            strncmp(fs_meta->entries[i].name, name, 12) == 0) {
            target_idx = i;
            break;
        }
    }
    // 2. Si no se encuentra, retorna -1.
    if (target_idx == -1) {
        printf("FS Error: Archivo '%s' no encontrado.\n", name);
        return -1;
    }
    // 3. Obtén el 'file_size' real del archivo desde los metadatos.
    uint32_t file_size = fs_meta->entries[target_idx].size;
    // 4. Determina cuánto vas a leer: el mínimo entre 'file_size' y 'max_size'.
    uint32_t to_read = (file_size < max_size) ? file_size : max_size;
    // 5. Calcula la dirección de memoria física: (XIP_BASE + offset).
    uint32_t address = XIP_BASE + fs_meta->entries[target_idx].offset;
    // 6. Usa memcpy() para copiar los bytes directamente de la dirección física al buffer.
    memcpy(buffer, (const void*)address, to_read);
    // 7. Retorna la cantidad de bytes leídos.
    return to_read;
}
    
// --- MÓDULO 4: Borrado Lógico & Visualización ---

int fs_delete(const char* name) {
    // 1. Busca el índice del archivo solicitado. Si no existe, retorna -1.
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

void fs_dump() {
    printf("\n=== Mapa de PicoFS ===\n");
    printf("Magic Number: 0x%08X\n", fs_meta->magic_number);
    printf("%-12s  %-10s | %-8s | %-8s\n", "Name", "Offset", "Size", "Status");
    printf("------------------------------------------------\n");

    // Volcado de la tabla
    // 1. Itera sobre las entradas de metadatos (MAX_FILES).
    // 2. Si el estado es distinto a STATUS_FREE, imprime sus campos con formato similar al encabezado.
    // 3. Para el estado, convierte los códigos Hex a strings (Ej. "OCCUPIED" o "DELETED").
    for (int i = 0; i < MAX_FILES; i++) {
        if (fs_meta->entries[i].status != STATUS_FREE) {
            const char* status_str = (fs_meta->entries[i].status == STATUS_OCCUPIED) ? "OCCUPIED" :
                                        (fs_meta->entries[i].status == STATUS_DELETED) ? "DELETED" : "UNKNOWN";
            printf("%-12s  0x%08X | %-8u | %-8s\n", fs_meta->entries[i].name, fs_meta->entries[i].offset, fs_meta->entries[i].size, status_str);
        }
    }

    printf("==============================\n\n");
}

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
 * @brief Escribe datos al final de un archivo existente. Implementa un esquema de read-modify-write para preservar los datos anteriores.
 * @param name Nombre del archivo a modificar.
 * @param data Puntero a los datos que se desean agregar al final del archivo.
 * @param size Tamaño en bytes de los datos a escribir.
 * @return 0 si la operación fue exitosa, -1 si el archivo no existe
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

    /*
     * Flash writes require page alignment.
     */

    uint32_t sectors_needed =
        (new_size + FLASH_SECTOR_SIZE - 1)
        / FLASH_SECTOR_SIZE;

    uint32_t total_size =
        sectors_needed * FLASH_SECTOR_SIZE;

    /*
     * Read old file contents
     */

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