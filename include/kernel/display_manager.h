/**
 * @file display_manager.h
 * @brief Driver del LCD 2004A (20x4) via I2C (PCF8574 + HD44780).
 *
 * Provee acceso privilegiado al bus I2C0 para controlar el display.
 * Las tareas de usuario solicitan actualizaciones via syscall.
 */

#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

/** Inicializa I2C y el LCD HD44780 via PCF8574 (llamar desde kernel boot) */
void display_manager_init(void);

/** Actualiza el contenido del LCD (parsea \n como separador de filas) */
void display_manager_update(const char *text);

#endif
