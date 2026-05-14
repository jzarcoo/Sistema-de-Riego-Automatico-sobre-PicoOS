/**
 * @file kernel_hw_config.h
 * @brief Mapa de pines fisicos del RP2040 para el sistema de riego.
 *
 * Conexionado:
 * - GPIO 8:  I2C0 SDA -> Display LCD 2004A (PCF8574)
 * - GPIO 9:  I2C0 SCL -> Display LCD 2004A (PCF8574)
 * - GPIO 6:  Salida digital -> Rele/bomba de riego (activa LOW)
 * - GPIO 14: Entrada digital -> Boton de riego manual (pull-down)
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
#define BUTTON_PIN               14

/* --- Display LCD 2004A (I2C0) --- */
#define DISPLAY_SDA_PIN          8
#define DISPLAY_SCL_PIN          9

#endif

