/**
 * @file kernel_hw_config.h
 * @brief Mapa de pines fisicos del RP2040 para el sistema de riego.
 *
 * Conexionado:
 * - GPIO 4:  I2C0 SDA -> Display OLED SSD1306
 * - GPIO 5:  I2C0 SCL -> Display OLED SSD1306
 * - GPIO 6:  Salida digital -> Rele/bomba de riego
 * - GPIO 16: Entrada digital -> Boton de riego manual (pull-up)
 * - GPIO 26: ADC0 -> Sensor de humedad del suelo
 */

#ifndef KERNEL_HW_CONFIG_H
#define KERNEL_HW_CONFIG_H

/* --- Bomba de riego --- */
#define IRRIGATION_PUMP_PIN      6

/* --- Sensor de humedad (ADC) --- */
#define SOIL_SENSOR_ADC_INPUT    0
#define SOIL_MOISTURE_SENSOR_PIN 26

/* --- Boton de riego manual --- */
#define BUTTON_PIN               16

/* --- Display OLED (I2C0) --- */
#define DISPLAY_SDA_PIN          4
#define DISPLAY_SCL_PIN          5

#endif

