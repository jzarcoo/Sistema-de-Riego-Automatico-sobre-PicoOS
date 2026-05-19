#include <stdio.h>
#include <stdint.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/exception.h"
#include "hardware/gpio.h"

#include "scheduler.h"
#include "syscalls.h"
#include "kernel_drivers.h"
#include "kernel_events.h"
#include "irrigation_manager.h"
#include "user_app.h"
#include "message_queue.h"

extern void irrigation_task(void);
extern void sensor_task(void);
extern void trigger_task(void);
extern void logger_task(void);
extern void display_task(void);

#define SYSTICK_BASE     0xE000E000
#define SYSTICK_CTRL (*(volatile uint32_t *)(SYSTICK_BASE + 0x10))
#define SYSTICK_LOAD (*(volatile uint32_t *)(SYSTICK_BASE + 0x14))
#define SYSTICK_VAL  (*(volatile uint32_t *)(SYSTICK_BASE + 0x18))

/* Minimal MPU Driver (Registers) */
#define MPU_TYPE   (*(volatile uint32_t*)0xE000ED90)
#define MPU_CTRL   (*(volatile uint32_t*)0xE000ED94)
#define MPU_RNR    (*(volatile uint32_t*)0xE000ED98)
#define MPU_RBAR   (*(volatile uint32_t*)0xE000ED9C)
#define MPU_RASR   (*(volatile uint32_t*)0xE000EDA0)

#define EXCEPTION_SVC     11
#define EXCEPTION_PENDSV  14
#define EXCEPTION_HARDFAULT  3

extern void wrapper_svc(void);
extern void isr_pendsv(void);

volatile uint32_t fault_count = 0;
volatile uint32_t last_fault_pc = 0;

void systick_init(uint32_t ticks) {
    SYSTICK_LOAD = ticks - 1;
    SYSTICK_VAL = 0;
    SYSTICK_CTRL = 0x07;
}

void HardFault_Handler_C(uint32_t *stack_frame) {
    int core_id = get_core_num();
    core_scheduler_t *sched = &core_schedulers[core_id];
    fault_count++;
    last_fault_pc = stack_frame[6];
    sched->tasks[sched->current_task].state = DORMANT;
    *(volatile uint32_t *)0xE000ED04 = (1 << 28);
}

void __attribute__((naked)) HardFault_Handler(void) {
    __asm volatile(
        "mov r0, lr            \n"
        "movs r1, #4           \n"
        "tst r0, r1            \n"
        "beq use_msp           \n"
        "mrs r0, psp           \n"
        "b call_c              \n"
        "use_msp:              \n"
        "mrs r0, msp           \n"
        "call_c:               \n"
        "b HardFault_Handler_C \n"
    );
}
void irrigation_task_update(void)
{
    while (1)
    {
        sys_heartbeat();
        irrigation_manager_update();


    }
}

void mpu_init(void) {
    __asm volatile("dmb");

    MPU_CTRL = 0;

    // Region 0: Flash (0x10000000, 16MB) — Full access, ejecutable
    MPU_RNR = 0;
    MPU_RBAR = 0x10000000;
    MPU_RASR = (3 << 24) |   // AP=011: Full access
               (23 << 1) |   // SIZE=23 → 16MB
               (1 << 0);

    // Region 1: RAM (0x20000000, 256KB) — Full access, no ejecutable
    MPU_RNR = 1;
    MPU_RBAR = 0x20000000;
    MPU_RASR = (1 << 28) |   // XN
               (3 << 24) |   // AP=011: Full access
               (1 << 18) |   // Shareable
               (17 << 1) |   // SIZE=17 → 256KB
               (1 << 0);

    // Region 2: Perifericos (0x40000000, 512MB) — Accesible por user
    // Abierto para que el SDK funcione (TIMER, UART, USB, DMA, etc.)
    MPU_RNR = 2;
    MPU_RBAR = 0x40000000;
    MPU_RASR = (1 << 28) |   // XN
               (3 << 24) |   // AP=011: Full access
               (1 << 18) |
               (28 << 1) |   // 512MB
               (1 << 0);

    // Region 3: SIO (0xD0000000, 256MB) — Accesible por user
    // Spinlocks del SDK y get_core_num()
    MPU_RNR = 3;
    MPU_RBAR = 0xD0000000;
    MPU_RASR = (1 << 28) |   // XN
               (3 << 24) |   // AP=011: Full access
               (1 << 18) |
               (27 << 1) |   // 256MB
               (1 << 0);

    // Region 4: IO_BANK0 (0x40014000, 16KB) — Solo kernel
    // Protege registros GPIO (pin de la bomba). User → HardFault.
    MPU_RNR = 4;
    MPU_RBAR = 0x40014000;
    MPU_RASR = (1 << 28) |   // XN
               (1 << 24) |   // AP=001: Solo privilegiado
               (1 << 18) |
               (13 << 1) |   // SIZE=13 → 16KB
               (1 << 0);

    // Region 5: PADS_BANK0 (0x4001C000, 4KB) — Solo kernel
    MPU_RNR = 5;
    MPU_RBAR = 0x4001C000;
    MPU_RASR = (1 << 28) |   // XN
               (1 << 24) |   // AP=001: Solo privilegiado
               (1 << 18) |
               (11 << 1) |   // SIZE=11 → 4KB
               (1 << 0);

    MPU_CTRL = 0; // MPU configurada pero NO activada (para debug)

    __asm volatile("dsb");
    __asm volatile("isb");
}

// ============================================================
// CORE 1: Monitoreo y control critico
// Tareas: sensor_task, irrigation_task, trigger_task
// ============================================================
void core1_entry(void) {
    // 
    multicore_lockout_victim_init();


    exception_set_exclusive_handler((enum exception_number)EXCEPTION_SVC, wrapper_svc);
    exception_set_exclusive_handler((enum exception_number)EXCEPTION_PENDSV, isr_pendsv);
    exception_set_exclusive_handler((enum exception_number)EXCEPTION_HARDFAULT, HardFault_Handler);

    core_schedulers[1].current_task = -1;
    core_schedulers[1].num_tasks = 0;

    // GPIO driver: configurar pin 14 como input con pull-down
    // (antes de MPU porque accede registros de IO_BANK0 directamente)
    gpio_init(14);
    gpio_set_dir(14, GPIO_IN);
    gpio_pull_down(14);

    // Event system: registrar IRQ rising edge → encolar MSG_MANUAL_TRIGGER
    k_gpio_event_system_init();
    k_gpio_event_register(14, GPIO_IRQ_EDGE_RISE, &irrigation_queue, MSG_MANUAL_TRIGGER);

    // MPU se activa DESPUÉS de configurar hardware
    mpu_init();

    task_create_on_core(1, 0, irrigation_task);
    task_create_on_core(1, 1, sensor_task);
    task_create_on_core(1, 2, trigger_task);
    task_create_on_core(1, 3, irrigation_task_update);

    printf("[CORE1] Iniciado - Monitoreo y control critico\n");

    __asm volatile ("msr psp, %0" : : "r" (0));

    systick_init(1250000);

    while (1) {
        //irrigation_manager_update();
        __asm volatile ("wfi");
    }
}

// ============================================================
// CORE 0: Planificador, UI y logs
// Tareas: logger_task, display_task
// ============================================================
int main() {
    stdio_init_all();
    // Esperar usb 
    while (!stdio_usb_connected()) {
        sleep_ms(100);
    }
    sleep_ms(500);
    printf("Conexion USB establecida.\n");

    // Inicializar schedulers antes de todo
    core_schedulers[0].current_task = -1;
    core_schedulers[0].num_tasks = 0;
    core_schedulers[1].current_task = -1;
    core_schedulers[1].num_tasks = 0;

    // Inicializar handlers de excepciones
    exception_set_exclusive_handler((enum exception_number)EXCEPTION_SVC, wrapper_svc);
    exception_set_exclusive_handler((enum exception_number)EXCEPTION_PENDSV, isr_pendsv);
    exception_set_exclusive_handler((enum exception_number)EXCEPTION_HARDFAULT, HardFault_Handler);

    // tareas del core0
    task_create_on_core(0, 0, logger_task);
    task_create_on_core(0, 1, display_task);

    mq_init(&irrigation_queue);

    printf("[CORE0] Iniciado - Planificador, UI y logs\n");

    multicore_launch_core1(core1_entry);
    sleep_ms(100);

    printf("[CORE0] Core1 lanzado. Iniciando scheduler...\n");

    // MPU se activa justo antes del scheduler, despues de toda la inicializacion
    mpu_init();

    __asm volatile ("msr psp, %0" : : "r" (0));

    systick_init(1250000);

    while (1) {
        __asm volatile ("wfi");
    }
}
