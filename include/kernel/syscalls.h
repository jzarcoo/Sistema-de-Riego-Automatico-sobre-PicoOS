/**
 * @file syscalls.h
 * @brief Interfaz de syscalls para tareas de usuario.
 *
 * Las tareas en modo no privilegiado usan estas funciones para
 * solicitar servicios al kernel via instrucciones SVC. Cada funcion
 * genera un trap al handler de SVC que despacha al servicio apropiado
 * en modo privilegiado.
 */

#ifndef SYSCALLS_H
#define SYSCALLS_H

#include "semaphore.h"
#include "message_queue.h"

/** Establece nivel de salida de un GPIO */
void sys_gpio_set(int pin, int value);

/** Lee nivel de entrada de un GPIO */
int sys_gpio_get(int pin);

/** Configura direccion de un GPIO (1=output, 0=input) */
int sys_gpio_dir(int pin, int output);

/** Inicializa un semaforo con valor inicial */
void sys_sem_init(kernel_semaphore_t *sem, int valor_inicial);

/** Adquiere un semaforo (bloquea si no hay recursos) */
void sys_sem_wait(kernel_semaphore_t *sem);

/** Libera un semaforo */
void sys_sem_post(kernel_semaphore_t *sem);

/** Habilita pull-up interno en un pin */
void sys_gpio_pullup(int pin);

/** Registra un pin como fuente de IRQ con cola destino */
void sys_gpio_irq_register(int pin, message_queue_t *queue, message_type_t msg_type);

/** Inicializa el ADC para el sensor de humedad */
void sys_adc_init(void);

/** Lee el valor del sensor de humedad (0-4095) */
int sys_read_soil_sensor(void);

/** Enciende la bomba (acceso directo, preferir sys_request_irrigation) */
void sys_pump_on(void);

/** Apaga la bomba */
void sys_pump_off(void);

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

/** Solicita una actualizacion del display con el texto dado */
void sys_request_display_update(const char* text);

#endif
