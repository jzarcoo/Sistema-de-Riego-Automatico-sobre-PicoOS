/**
 * @file mpu.h
 * @brief Interfaz de la Memory Protection Unit (MPU) del RP2040.
 *
 * Configura regiones de proteccion de memoria para aislar el acceso
 * a perifericos criticos (GPIO, PADS, I2C) del modo no privilegiado.
 */

#ifndef MPU_H
#define MPU_H

/**
 * Inicializa la MPU con regiones de proteccion para perifericos.
 * Debe llamarse una vez por core, despues de inicializar hardware
 * y antes de arrancar el scheduler.
 */
void mpu_init(void);

#endif
