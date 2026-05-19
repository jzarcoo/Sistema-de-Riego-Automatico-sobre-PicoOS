#ifndef KERNEL_DRIVERS_H
#define KERNEL_DRIVERS_H

#include <stdint.h>

/*
    Inicializa el pin
*/
void k_gpio_init(uint32_t pin, uint32_t output);


void k_gpio_set(uint32_t pin, uint32_t value);
int k_gpio_get(uint32_t pin);
void k_gpio_pullup(uint32_t pin);
void k_gpio_pulldown(uint32_t pin);
void k_gpio_irq_enable(uint32_t pin, uint32_t rising_edge);
void k_gpio_irq_clear(uint32_t pin);

void kernel_pump_on(void);
void kernel_pump_off(void);
int kernel_read_soil_sensor(void);
void k_adc_init(void);

#endif
