/**
 * @file irrigation_manager.h
 * @brief Driver de riego a nivel kernel.
 */

#ifndef IRRIGATION_MANAGER_H
#define IRRIGATION_MANAGER_H

/** Inicializa hardware de la bomba, ADC y boton (llamar desde kernel boot) */
void irrigation_manager_init(void);

/** Solicita un ciclo de riego (non-blocking) */
void irrigation_manager_request_water(void);

/** Maquina de estados del riego (llamar periodicamente desde kernel) */
void irrigation_manager_update(void);

#endif
