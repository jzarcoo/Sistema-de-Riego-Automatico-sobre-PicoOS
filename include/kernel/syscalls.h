/**
 * @file syscalls.h
 * @brief Interfaz de syscalls para tareas de usuario.
 *
 * Las tareas en modo no privilegiado usan estas funciones para
 * solicitar servicios al kernel via instrucciones SVC.
 */

#ifndef SYSCALLS_H
#define SYSCALLS_H

#include "semaphore.h"
#include "message_queue.h"

/** Establece nivel de salida de un GPIO */
void sys_gpio_set(int pin, int value);

/** Lee nivel de entrada de un GPIO */
int sys_gpio_get(int pin);

/** Inicializa un semaforo con valor inicial */
void sys_sem_init(kernel_semaphore_t *sem, int valor_inicial);

/** Adquiere un semaforo (bloquea si no hay recursos) */
void sys_sem_wait(kernel_semaphore_t *sem);

/** Libera un semaforo */
void sys_sem_post(kernel_semaphore_t *sem);

/** Lee el valor del sensor de humedad (0-4095) */
int sys_read_soil_sensor(void);

/** Solicita un ciclo de riego al irrigation_manager */
void sys_request_irrigation(void);

/** Reporta heartbeat al watchdog del kernel */
void sys_heartbeat(void);

/** Bloquea la tarea actual por ms milisegundos */
void sys_sleep(uint32_t ms);

/** Termina la tarea actual */
void sys_exit(void);

/** Imprime texto por serial (via kernel) */
void sys_print(const char* str);

/** Escribe texto en una fila del display (no hace I2C, retorna inmediato) */
void sys_display_write(int row, const char* text);

/** Flush: envia el buffer al LCD via I2C */
void sys_display_flush(void);

/** Ejecuta irrigation_manager_update en modo privilegiado (accede GPIO) */
void sys_irrigation_update(void);

/** Inicializa filesystem + page cache (accede Flash, requiere privilegio) */
void sys_logger_init(void);

/** Escribe texto en la page cache del log */
void sys_log_write(const char* text);

/** Flush de paginas dirty a Flash */
void sys_log_flush(void);

#endif
