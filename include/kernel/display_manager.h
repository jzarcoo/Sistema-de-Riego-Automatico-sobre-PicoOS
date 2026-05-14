/**
 * @file display_manager.h
 * @brief Driver del LCD 2004A (20x4) via I2C (PCF8574 + HD44780).
 *
 * Layout fijo de pantalla:
 *   Fila 0: HUMEDAD: xx%  ESTADO
 *   Fila 1: BOMBA: ON/OFF
 *   Fila 2: ultimo evento (log)
 *   Fila 3: ultimo evento (log)
 */

#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#define DISPLAY_ROW_HUMIDITY  0
#define DISPLAY_ROW_PUMP      1
#define DISPLAY_ROW_EVENT1    2

/** Inicializa I2C y el LCD (llamar desde kernel boot) */
void display_manager_init(void);

/** Actualiza una fila especifica del LCD (solo copia, no I2C) */
void display_manager_set_row(int row, const char *text);

/** Flush: envia al LCD las filas que cambiaron */
void display_manager_refresh(void);

#endif
