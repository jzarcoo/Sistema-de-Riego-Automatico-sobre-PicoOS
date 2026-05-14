#include <stdint.h>
#include <stdio.h>
#include "hardware/adc.h"
//#include "pico/stdlib.h"

#include<user_app.h>

/* SIO base address for GPIO control on the RP2040 */
//#define SIO_BASE 0xd0000000
#define SIO_GPIO_IN (SIO_BASE + 0x004)
#define SIO_GPIO_OUT_SET (SIO_BASE + 0x014)
#define SIO_GPIO_OUT_CLR (SIO_BASE + 0x018)
#define SIO_GPIO_OE_SET (SIO_BASE + 0x024)
#define SIO_GPIO_OE_CLR (SIO_BASE + 0x028)

/* IO bank 0 base address */
// #define IO_BANK0_BASE 0x40014000
#define IO_BANK0_GPIO_CTRL(pin) (IO_BANK0_BASE + 0x004 + (pin) * 8)
#define GPIO_FUNC_SIO 5

/* PADS Control for Pull-Up/Down */
// #define PADS_BANK0_BASE 0x4001c000
#define PADS_GPIO(x) (PADS_BANK0_BASE + 0x04 + (x)*4)


/* * INTERRUPT REGISTERS (PROC0)
 * INTE0: Pins 0-7   (Offset 0x100)
 * INTE1: Pins 8-15  (Offset 0x104)
 * INTE2: Pins 16-23 (Offset 0x108)
 * INTE3: Pins 24-29 (Offset 0x10C)
 */
#define IO_BANK0_PROC0_INTE0_BASE (IO_BANK0_BASE + 0x100)

/* * RAW INTERRUPT STATUS (INTR)
 * INTR0: Pins 0-7   (Offset 0x0F0)
 * INTR1: Pins 8-15  (Offset 0x0F4)
 * ...
 */
#define IO_BANK0_INTR0_BASE (IO_BANK0_BASE + 0x0F0)

/**
 * @brief Initialize a GPIO pin for SIO and set its direction.
 * @param pin GPIO number.
 * @param output 1 for output, 0 for input.
 */
void k_gpio_init(uint32_t pin, uint32_t output)
{
    volatile uint32_t *gpio_ctrl = (volatile uint32_t *)IO_BANK0_GPIO_CTRL(pin);
    *gpio_ctrl = (*gpio_ctrl & ~0x1F) | GPIO_FUNC_SIO;

    if (output)
        *(volatile uint32_t *)SIO_GPIO_OE_SET = (1 << pin);
    else
        *(volatile uint32_t *)SIO_GPIO_OE_CLR = (1 << pin);
}

/**
 * @brief Set a GPIO output level.
 * @param pin GPIO number.
 * @param value 1 to set high, 0 to set low.
 */
void k_gpio_set(uint32_t pin, uint32_t value)
{
    if (value)
        *(volatile uint32_t *)SIO_GPIO_OUT_SET = (1 << pin);
    else
        *(volatile uint32_t *)SIO_GPIO_OUT_CLR = (1 << pin);
}

/**
 * @brief Read a GPIO input level.
 * @param pin GPIO number.
 * @return 1 if high, 0 if low.
 */
int k_gpio_get(uint32_t pin)
{
    uint32_t state = *(volatile uint32_t *)SIO_GPIO_IN;
    return (state >> pin) & 1;
}

/**
 * @brief Enable internal pull-up to avoid floating pins.
 */
void k_gpio_pullup(uint32_t pin) {
    volatile uint32_t *pad_ctrl = (volatile uint32_t *)PADS_GPIO(pin);
    // Bit 3 = PUE (Pull Up Enable), Bit 2 = PDE (Pull Down Enable)
    *pad_ctrl |= (1 << 3); 
    *pad_ctrl &= ~(1 << 2);
}

/**
 * @brief Enable edge interrupt mapped by 8-pin banks.
 * @param pin GPIO number.
 * @param rising_edge 1 for rising edge, 0 for falling edge.
 */
void k_gpio_irq_enable(uint32_t pin, uint32_t rising_edge) {
    // Compute which INTE register this pin maps to (0, 1, 2, or 3)
    // Pin 15 -> Bank 1
    uint32_t bank_idx = pin / 8;
    
    // Compute the shift within that register (0 to 7)
    // Pin 15 -> Shift 7 (within the bank)
    uint32_t pin_in_bank = pin % 8;
    
    // Each pin uses 4 bits, so multiply by 4
    // Pin 15 -> Bits 28-31 of INTE1
    uint32_t shift = pin_in_bank * 4;

    // Get address of the correct register
    volatile uint32_t *inte_reg = (volatile uint32_t *)(IO_BANK0_PROC0_INTE0_BASE + (bank_idx * 4));

    // Write configuration (+2 Edge Low, +3 Edge High)
    if (rising_edge) {
        *inte_reg |= (1 << (shift + 3)); 
    } else {
        *inte_reg |= (1 << (shift + 2)); 
    }
}

/**
 * @brief Clear the interrupt using the correct INTR register.
 * @param pin GPIO number.
 */
void k_gpio_irq_clear(uint32_t pin) {
    uint32_t bank_idx = pin / 8;
    uint32_t pin_in_bank = pin % 8;
    uint32_t shift = pin_in_bank * 4;

    // INTR0 base + bank offset
    volatile uint32_t *intr_reg = (volatile uint32_t *)(IO_BANK0_INTR0_BASE + (bank_idx * 4));
    
    // Clear both edges by writing 1s (W1C)
    *intr_reg = (1 << (shift + 2)) | (1 << (shift + 3));
}

void kernel_pump_on(void) {
    k_gpio_set(IRRIGATION_PUMP_PIN, 1);
}

void kernel_pump_off(void) {
    k_gpio_set(IRRIGATION_PUMP_PIN, 0);
}

/**
 * @brief Inicializa el ADC para leer el sensor de humedad del suelo. 
 * Configura el pin correspondiente y selecciona la entrada ADC.
 */
void k_adc_init(void) {
    adc_init();
    adc_gpio_init(SOIL_MOISTURE_SENSOR_PIN);
    adc_select_input(0);
}

/**
 * @brief Lee el valor del sensor de humedad del suelo a través del ADC.
 * @return Valor de humedad (0-4095).
 */
int kernel_read_soil_sensor(void){
    int value = adc_read();
    return value;
}
