/**
 * @file mpu_test_task.c
 * @brief Tarea de demostracion de tolerancia a fallos.
 *
 * Ejecuta 3 tests secuenciales para la presentacion:
 *
 * TEST 1 - Violacion MPU:
 *   Intenta escribir a IO_BANK0. La MPU genera HardFault.
 *   El handler mata la tarea. El watchdog la reinicia.
 *
 * TEST 2 - Watchdog timeout:
 *   Deja de reportar heartbeat. Despues de 5 segundos,
 *   el watchdog detecta la tarea colgada y la reinicia.
 *
 * TEST 3 - Sistema sigue vivo:
 *   Despues de 2 muertes y 2 reinicios, la tarea reporta
 *   que el sistema sobrevivio a los fallos.
 *
 * La tarea usa un contador estatico para saber en que
 * test va (sobrevive reinicios porque es variable global).
 */

#include "syscalls.h"
#include <stdint.h>

#define IO_BANK0_BASE_ADDR 0x40014000

static int test_phase = 0;

void mpu_test_task(void) {
    test_phase++;

    if (test_phase == 1) {
        sys_print("\n=== TEST MPU: Violacion de memoria ===\n");
        sys_print("[TEST1] Esperando 10s antes de violar MPU...\n");
        sys_sleep(10000);

        sys_print("[TEST1] Escribiendo a IO_BANK0 (0x40014000)...\n");
        sys_print("[TEST1] -> HardFault esperado AHORA\n");

        volatile uint32_t *gpio_reg = (volatile uint32_t *)IO_BANK0_BASE_ADDR;
        *gpio_reg = 0xDEADBEEF;

        sys_print("[TEST1] ERROR: MPU no bloqueo!\n");
    }

    if (test_phase == 2) {
        sys_print("\n=== TEST WATCHDOG: Tarea colgada ===\n");
        sys_print("[TEST2] Dejando de reportar heartbeat...\n");
        sys_print("[TEST2] El watchdog deberia matarme en ~5s\n");

        while (1) {
            /* No llamar sys_heartbeat() a proposito */
        }
    }

    if (test_phase >= 3) {
        sys_print("\n=== RESULTADO: Sistema sobrevivio ===\n");
        sys_print("[TEST3] 2 fallos, 2 reinicios, sistema intacto.\n");
        sys_print("[TEST3] Logger, display y sensor siguen vivos.\n");
        sys_print("=== FIN DE TESTS ===\n\n");
    }

    while (1) {
        sys_heartbeat();
    }
}
