#include <stdint.h>
#include <stdio.h>
//#include "pico/stdlib.h"

#include<user_app.h>

/* SIO base address for GPIO control on the RP2040 */
#define SIO_BASE 0xd0000000
#define SIO_GPIO_IN (SIO_BASE + 0x004)
#define SIO_GPIO_OUT_SET (SIO_BASE + 0x014)
#define SIO_GPIO_OUT_CLR (SIO_BASE + 0x018)
#define SIO_GPIO_OE_SET (SIO_BASE + 0x024)
#define SIO_GPIO_OE_CLR (SIO_BASE + 0x028)

/* IO bank 0 base address */
#define IO_BANK0_BASE 0x40014000
#define IO_BANK0_GPIO_CTRL(pin) (IO_BANK0_BASE + 0x004 + (pin) * 8)
#define GPIO_FUNC_SIO 5

/* PADS Control for Pull-Up/Down */
#define PADS_BANK0_BASE 0x4001c000
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



/* ADC Registers */

/* =========================
   REGISTROS RP2040
   ========================= */

#define REG32(addr) (*(volatile uint32_t *)(addr))

/* ADC */
#define ADC_BASE 0x4004C000u

#define ADC_CS      REG32(ADC_BASE + 0x00)
#define ADC_RESULT  REG32(ADC_BASE + 0x04)


/* ADC_CS bits */
#define ADC_CS_EN           (1 << 0)
#define ADC_CS_START_ONCE   (1 << 2)
#define ADC_CS_READY        (1 << 8)

/* IO BANK */
//#define IO_BANK0_BASE 0x40014000u

#define GPIO26_CTRL REG32(IO_BANK0_BASE + 0xD4)

/* PADS BANK */
//#define PADS_BANK0_BASE 0x4001C000u

#define PAD26 REG32(PADS_BANK0_BASE + 0x6C)


// #define ADC_BASE        0x4004C000

// #define ADC_CS          (*(volatile uint32_t *)(ADC_BASE + 0x00))
// #define ADC_RESULT      (*(volatile uint32_t *)(ADC_BASE + 0x04))

// #define ADC_CS_EN               (1 << 0)
// #define ADC_CS_START_ONCE       (1 << 3)
// #define ADC_CS_READY            (1 << 8)
// #define ADC_CS_AINSEL_LSB       12
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


void k_adc_init(void)
{
    /* Configure GPIO26 as ADC function */

    volatile uint32_t *gpio_ctrl =
        (volatile uint32_t *)IO_BANK0_GPIO_CTRL(26);

    /* FUNCSEL = 0 -> ADC */
    *gpio_ctrl &= ~0x1F;

    /* Disable pull-up / pull-down */
    volatile uint32_t *pad =
        (volatile uint32_t *)PADS_GPIO(26);

    *pad &= ~((1 << 2) | (1 << 3));

    /* Enable ADC hardware */
    ADC_CS |= ADC_CS_EN;
}
uint16_t k_adc_read(void)
{
    /* Select ADC0 = GPIO26 */
    ADC_CS &= ~(0x7 << 12);

    /* Start conversion */
    ADC_CS |= ADC_CS_START_ONCE;

    /* Wait until conversion finishes */
    while (!(ADC_CS & ADC_CS_READY));

    /* Read 12-bit ADC value */
    return ADC_RESULT & 0xFFF;
}

int kernel_read_soil_sensor(void)
{
    return (int)k_adc_read();
}
