/**
 * @file kernel_drivers.c
 * @brief Drivers de bajo nivel para GPIO y ADC (acceso directo a registros).
 *
 * Provee la capa de abstraccion de hardware a nivel kernel. Accede
 * directamente a los registros SIO, IO_BANK0 y PADS_BANK0 del RP2040
 * para controlar pines GPIO sin depender de la HAL del SDK (excepto ADC).
 * Estos drivers solo son accesibles desde modo privilegiado; las tareas
 * de usuario deben usar syscalls para interactuar con el hardware.
 *
 * Registros utilizados:
 * - SIO (0xD0000000): Control rapido de salida/entrada GPIO.
 * - IO_BANK0 (0x40014000): Seleccion de funcion de pin (SIO, UART, etc).
 * - PADS_BANK0 (0x4001C000): Configuracion electrica (pull-up/down).
 */

#include <stdint.h>
#include "hardware/adc.h"
#include "kernel_hw_config.h"

/* Direcciones base del SIO para control GPIO */
#define SIO_GPIO_IN (SIO_BASE + 0x004)
#define SIO_GPIO_OUT_SET (SIO_BASE + 0x014)
#define SIO_GPIO_OUT_CLR (SIO_BASE + 0x018)
#define SIO_GPIO_OE_SET (SIO_BASE + 0x024)
#define SIO_GPIO_OE_CLR (SIO_BASE + 0x028)

/* Direcciones de IO_BANK0 para funcion de pin */
#define IO_BANK0_GPIO_CTRL(pin) (IO_BANK0_BASE + 0x004 + (pin) * 8)
#define GPIO_FUNC_SIO 5

/* Direcciones de PADS_BANK0 para resistencias pull */
#define PADS_GPIO(x) (PADS_BANK0_BASE + 0x04 + (x)*4)


/**
 * @brief Inicializa un pin GPIO para funcion SIO y configura direccion.
 *
 * Escribe en IO_BANK0_GPIO_CTRL para seleccionar funcion SIO (5),
 * luego habilita o deshabilita la salida via SIO_GPIO_OE.
 *
 * @param pin Numero de GPIO (0-29).
 * @param output 1 para configurar como salida, 0 como entrada.
 */
void k_gpio_init(uint32_t pin, uint32_t output)
{
    volatile uint32_t *gpio_ctrl = (volatile uint32_t *)IO_BANK0_GPIO_CTRL(pin);
    *gpio_ctrl = (*gpio_ctrl & ~0x1F) | GPIO_FUNC_SIO;

    /* Habilitar input en pad (bit 6 = IE) y desactivar output disable (bit 7 = OD) */
    volatile uint32_t *pad_ctrl = (volatile uint32_t *)PADS_GPIO(pin);
    *pad_ctrl = (*pad_ctrl | (1 << 6)) & ~(1 << 7);

    if (output)
        *(volatile uint32_t *)SIO_GPIO_OE_SET = (1 << pin);
    else
        *(volatile uint32_t *)SIO_GPIO_OE_CLR = (1 << pin);
}

/**
 * @brief Establece el nivel de salida de un pin GPIO.
 *
 * Escribe en SIO_GPIO_OUT_SET o SIO_GPIO_OUT_CLR segun el valor.
 *
 * @param pin Numero de GPIO.
 * @param value 1 para nivel alto, 0 para nivel bajo.
 */
void k_gpio_set(uint32_t pin, uint32_t value)
{
    if (value)
        *(volatile uint32_t *)SIO_GPIO_OUT_SET = (1 << pin);
    else
        *(volatile uint32_t *)SIO_GPIO_OUT_CLR = (1 << pin);
}

/**
 * @brief Lee el nivel de entrada de un pin GPIO.
 *
 * Lee el registro SIO_GPIO_IN y extrae el bit correspondiente.
 *
 * @param pin Numero de GPIO.
 * @return 1 si el pin esta en alto, 0 si esta en bajo.
 */
int k_gpio_get(uint32_t pin)
{
    uint32_t state = *(volatile uint32_t *)SIO_GPIO_IN;
    return (state >> pin) & 1;
}

/**
 * @brief Habilita resistencia pull-up interna en un pin.
 *
 * Modifica PADS_BANK0: activa bit 3 (pull-up) y desactiva bit 2 (pull-down).
 *
 * @param pin Numero de GPIO.
 */
void k_gpio_pullup(uint32_t pin) {
    volatile uint32_t *pad_ctrl = (volatile uint32_t *)PADS_GPIO(pin);
    *pad_ctrl |= (1 << 3);
    *pad_ctrl &= ~(1 << 2);
}

/**
 * @brief Habilita resistencia pull-down interna en un pin.
 *
 * Modifica PADS_BANK0: activa bit 2 (pull-down) y desactiva bit 3 (pull-up).
 *
 * @param pin Numero de GPIO.
 */
void k_gpio_pulldown(uint32_t pin) {
    volatile uint32_t *pad_ctrl = (volatile uint32_t *)PADS_GPIO(pin);
    *pad_ctrl |= (1 << 2);
    *pad_ctrl &= ~(1 << 3);
}


/**
 * @brief Inicializa el ADC para el sensor de humedad del suelo.
 *
 * Usa la HAL del SDK para configurar el periferico ADC y seleccionar
 * la entrada 0 (GPIO 26).
 */
void k_adc_init(void) {
    adc_init();
    adc_gpio_init(SOIL_MOISTURE_SENSOR_PIN);
    adc_select_input(0);
}

/**
 * @brief Lee el valor del sensor de humedad del suelo.
 *
 * Realiza una conversion ADC de 12 bits.
 *
 * @return Valor de humedad (0-4095). Mayor = mas seco.
 */
int kernel_read_soil_sensor(void){
    int value = adc_read();
    return value;
}
