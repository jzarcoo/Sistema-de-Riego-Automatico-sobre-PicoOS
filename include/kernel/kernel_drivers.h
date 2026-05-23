/**
 * @file kernel_drivers.h
 * @brief Interfaz de drivers de bajo nivel para GPIO y ADC.
 *
 * Funciones accesibles solo desde modo privilegiado (kernel).
 * Las tareas de usuario deben acceder mediante syscalls.
 */

#ifndef KERNEL_DRIVERS_H
#define KERNEL_DRIVERS_H

#include <stdint.h>

/** Inicializa un pin GPIO como SIO y configura direccion */
void k_gpio_init(uint32_t pin, uint32_t output);

/** Establece el nivel de salida de un pin */
void k_gpio_set(uint32_t pin, uint32_t value);

/** Lee el nivel de entrada de un pin */
int k_gpio_get(uint32_t pin);

/** Habilita pull-up interno en un pin */
void k_gpio_pullup(uint32_t pin);

/** Habilita pull-down interno en un pin */
void k_gpio_pulldown(uint32_t pin);

/** Habilita interrupcion por flanco en un pin */
void k_gpio_irq_enable(uint32_t pin, uint32_t rising_edge);

/** Limpia la interrupcion pendiente de un pin */
void k_gpio_irq_clear(uint32_t pin);

/** Enciende la bomba de riego */
void kernel_pump_on(void);

/** Apaga la bomba de riego */
void kernel_pump_off(void);

/** Lee el sensor de humedad del suelo via ADC */
int kernel_read_soil_sensor(void);

/** Inicializa el periferico ADC */
void k_adc_init(void);

#endif
