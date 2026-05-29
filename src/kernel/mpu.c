/**
 * @file mpu.c
 * @brief Configuracion de la Memory Protection Unit (MPU) del RP2040.
 *
 * Protege perifericos criticos del acceso no privilegiado (user tasks).
 * El kernel mantiene acceso total via PRIVDEFENA=1.
 *
 * Regiones:
 * - Region 0: Espacio completo 4GB, acceso full (background).
 * - Region 1: IO_BANK0 (0x40014000, 16KB) solo privilegiado.
 * - Region 2: PADS_BANK0 (0x4001C000, 4KB) solo privilegiado.
 * - Region 3: I2C0 (0x40044000, 4KB) solo privilegiado.
 * - Region 4: UART0 (0x40034000, 4KB) solo privilegiado.
 */

#include <stdint.h>

#define MPU_CTRL   (*(volatile uint32_t*)0xE000ED94)
#define MPU_RNR    (*(volatile uint32_t*)0xE000ED98)
#define MPU_RBAR   (*(volatile uint32_t*)0xE000ED9C)
#define MPU_RASR   (*(volatile uint32_t*)0xE000EDA0)

/** @brief Configura regiones MPU y activa proteccion con PRIVDEFENA. */
void mpu_init(void) {
    __asm volatile("dmb");

    MPU_CTRL = 0;

    MPU_RNR = 0;
    MPU_RBAR = 0x00000000;
    MPU_RASR = (3 << 24) |   // AP=011: Full access
               (31 << 1) |   // SIZE=31: 4GB
               (1 << 0);     // ENABLE

    MPU_RNR = 1;
    MPU_RBAR = 0x40014000;
    MPU_RASR = (1 << 28) |   // XN
               (1 << 24) |   // AP=001: Solo privilegiado
               (13 << 1) |   // SIZE=13: 16KB
               (1 << 0);     // ENABLE

    MPU_RNR = 2;
    MPU_RBAR = 0x4001C000;
    MPU_RASR = (1 << 28) |   // XN
               (1 << 24) |   // AP=001: Solo privilegiado
               (11 << 1) |   // SIZE=11: 4KB
               (1 << 0);     // ENABLE

    MPU_RNR = 3;
    MPU_RBAR = 0x40044000;
    MPU_RASR = (1 << 28) |   // XN
               (1 << 24) |   // AP=001: Solo privilegiado
               (11 << 1) |   // SIZE=11: 4KB
               (1 << 0);     // ENABLE

    // Region 4: UART0 — solo kernel (fuerza sys_print)
    MPU_RNR = 4;
    MPU_RBAR = 0x40034000;
    MPU_RASR = (1 << 28) |   // XN
               (1 << 24) |   // AP=001: Solo privilegiado
               (11 << 1) |   // SIZE=11: 4KB
               (1 << 0);     // ENABLE

    MPU_CTRL = (1 << 0) | (1 << 2);  // ENABLE + PRIVDEFENA

    __asm volatile("dsb");
    __asm volatile("isb");
}
