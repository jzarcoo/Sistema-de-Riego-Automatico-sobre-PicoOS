#ifndef SYSCALLS_H
#define SYSCALLS_H

#include "semaphore.h"

/**
 * @file syscalls.h
 * @brief Syscall interfaces for GPIO operations.
 */

/**
 * @brief Syscalls interface for gpio write operation.
 * @param pin GPIO pin number
 * @param value 1 for high, 0 for low
 */
void sys_gpio_set(int pin, int value);

/**
 * @brief Syscalls interface for gpio read operation.
 * @param pin GPIO pin number
 * @return 1 if high, 0 if low
 */
int sys_gpio_get(int pin);

/**
 * @brief Syscalls interface for gpio direction configuration.
 * @param pin GPIO pin number
 * @param output 1 for output, 0 for input
 * @return 0 on success, -1 on error (e.g., invalid pin)
 */
int sys_gpio_dir(int pin, int output);

/**
 * @brief Syscalls interface for semaphore initialization.
 * @param sem Pointer to the semaphore to initialize.
 * @param valor_inicial Initial value for the semaphore.
 */
void sys_sem_init(semaphore_t *sem, int valor_inicial);

/**
 * @brief Syscalls interface for semaphore wait operation.
 * @param sem Pointer to the semaphore to wait on.
 */
void sys_sem_wait(semaphore_t *sem);

/**
 * @brief Syscalls interface for semaphore post operation.
 * @param sem Pointer to the semaphore to post.
 */
void sys_sem_post(semaphore_t *sem);

/**
 * @brief Syscalls interface to enable internal pull-up resistor on a GPIO pin.
 * @param pin GPIO pin number
 */
void sys_gpio_pullup(int pin);

/**
 * @brief Syscalls interface to initialize the ADC for soil moisture sensor.
 */
void sys_adc_init(void);

/**
 * @brief Syscalls interface to read the soil moisture sensor value.
 * @return Sensor value 
 */
int sys_read_soil_sensor(void);

void sys_pump_on(void);
void sys_pump_off(void);
void sys_log_event(const char* event);
void sys_heartbeat(void);

/**
 * @brief Syscalls interface to terminate a task.
 */
void sys_exit(void);

#endif