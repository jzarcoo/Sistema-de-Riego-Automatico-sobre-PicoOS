/**
 * @file kernel_drivers.h
 * @brief Drivers de bajo nivel para GPIO y ADC.
 *
 * Solo accesible desde modo privilegiado (kernel).
 */

#ifndef KERNEL_DRIVERS_H
#define KERNEL_DRIVERS_H

#include <stdint.h>

void k_gpio_init(uint32_t pin, uint32_t output);
void k_gpio_set(uint32_t pin, uint32_t value);
int k_gpio_get(uint32_t pin);
void k_gpio_pullup(uint32_t pin);
void k_gpio_pulldown(uint32_t pin);

void k_adc_init(void);
int kernel_read_soil_sensor(void);

#endif
