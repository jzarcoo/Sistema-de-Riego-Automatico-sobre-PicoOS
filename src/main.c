/**
 * @file main.c
 * @brief Punto de entrada del sistema PicoOS — kernel boot dual-core.
 *
 * Arquitectura asimetrica del RP2040:
 * - Core 0: Planificador de gestion (display, logs).
 * - Core 1: Plano critico (sensor, riego, trigger).
 *
 * Secuencia de boot por core:
 * 1. Inicializar exception handlers.
 * 2. Inicializar hardware via managers (privilegiado).
 * 3. Inicializar recursos IPC (semaforos, colas).
 * 4. Activar MPU (protege perifericos del user space).
 * 5. Crear tareas y arrancar scheduler via SysTick.
 */

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
#include "display_manager.h"
#include "mpu.h"
#include "kernel_hw_config.h"
#include "user_app.h"
#include "message_queue.h"
#include "semaphore.h"

extern void irrigation_task(void);
extern void sensor_task(void);
extern void trigger_task(void);
extern void logger_task(void);
extern void display_task(void);

#define SYSTICK_BASE     0xE000E000
#define SYSTICK_CTRL (*(volatile uint32_t *)(SYSTICK_BASE + 0x10))
#define SYSTICK_LOAD (*(volatile uint32_t *)(SYSTICK_BASE + 0x14))
#define SYSTICK_VAL  (*(volatile uint32_t *)(SYSTICK_BASE + 0x18))

#define EXCEPTION_SVC       11
#define EXCEPTION_PENDSV    14
#define EXCEPTION_HARDFAULT  3

extern void wrapper_svc(void);
extern void isr_pendsv(void);

volatile uint32_t fault_count = 0;
volatile uint32_t last_fault_pc = 0;

static void systick_init(uint32_t ticks) {
    SYSTICK_LOAD = ticks - 1;
    SYSTICK_VAL = 0;
    SYSTICK_CTRL = 0x07;
}

void __attribute__((naked)) secure_waitt(void) {
    while (1) {
        __asm volatile("wfi");
    }
}

void HardFault_Handler_C(uint32_t *stack_frame) {
    int core_id = get_core_num();
    core_scheduler_t *sched = &core_schedulers[core_id];
    fault_count++;
    last_fault_pc = stack_frame[6];
    printf("[HardFault] Core %d - Count: %u, PC: 0x%08X\n",
           core_id, fault_count, last_fault_pc);
    if (sched->current_task != -1) {
        sched->tasks[sched->current_task].state = DORMANT;
    }
    *(volatile uint32_t *)0xE000ED04 = (1 << 28);
    stack_frame[6] = (uint32_t)secure_waitt;
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

/**
 * Tarea kernel que ejecuta la maquina de estados del riego.
 * Corre como tarea planificada en Core 1.
 */
static void irrigation_update_task(void) {
    while (1) {
        sys_heartbeat();
        irrigation_manager_update();
    }
}

/* ================================================================
 * Core 1: Plano critico — sensor, riego, trigger
 * ================================================================ */
void core1_entry(void) {
    multicore_lockout_victim_init();

    exception_set_exclusive_handler((enum exception_number)EXCEPTION_SVC, wrapper_svc);
    exception_set_exclusive_handler((enum exception_number)EXCEPTION_PENDSV, isr_pendsv);
    exception_set_exclusive_handler((enum exception_number)EXCEPTION_HARDFAULT, HardFault_Handler);

    core_schedulers[1].current_task = -1;
    core_schedulers[1].num_tasks = 0;

    /* --- Hardware init (privilegiado, antes de MPU) --- */
    irrigation_manager_init();
    k_gpio_event_system_init();
    k_gpio_event_register(BUTTON_PIN, GPIO_IRQ_EDGE_RISE,
                          &irrigation_queue, MSG_MANUAL_TRIGGER);

    /* --- Recursos IPC (kernel crea, user tasks usan) --- */
    k_sem_init(&irrigation_pump_sem, 1);

    /* --- Activar proteccion de memoria --- */
    mpu_init();

    /* --- Crear tareas --- */
    task_create_on_core(1, 0, irrigation_task);
    task_create_on_core(1, 1, sensor_task);
    task_create_on_core(1, 2, trigger_task);
    task_create_on_core(1, 3, irrigation_update_task);

    printf("[CORE1] Iniciado - Plano critico\n");

    __asm volatile ("msr psp, %0" : : "r" (0));
    systick_init(1250000);

    while (1) {
        __asm volatile ("wfi");
    }
}

/* ================================================================
 * Core 0: Plano de gestion — display, logs
 * ================================================================ */
int main() {
    stdio_init_all();
    while (!stdio_usb_connected()) {
        sleep_ms(100);
    }
    sleep_ms(500);
    printf("Conexion USB establecida.\n");

    /* Inicializar schedulers */
    core_schedulers[0].current_task = -1;
    core_schedulers[0].num_tasks = 0;
    core_schedulers[1].current_task = -1;
    core_schedulers[1].num_tasks = 0;

    /* Exception handlers */
    exception_set_exclusive_handler((enum exception_number)EXCEPTION_SVC, wrapper_svc);
    exception_set_exclusive_handler((enum exception_number)EXCEPTION_PENDSV, isr_pendsv);
    exception_set_exclusive_handler((enum exception_number)EXCEPTION_HARDFAULT, HardFault_Handler);

    /* --- Hardware init (privilegiado, antes de MPU) --- */
    display_manager_init();

    /* --- Recursos IPC (kernel crea, user tasks usan) --- */
    mq_init(&irrigation_queue);
    mq_init(&log_queue);
    mq_init(&display_queue);
    k_sem_init(&logger_sem, 1);
    k_sem_init(&display_sem, 1);

    /* --- Crear tareas --- */
    task_create_on_core(0, 0, logger_task);
    task_create_on_core(0, 1, display_task);

    printf("[CORE0] Iniciado - Planificador, UI y logs\n");

    /* Lanzar Core 1 */
    multicore_launch_core1(core1_entry);
    sleep_ms(100);
    printf("[CORE0] Core1 lanzado. Iniciando scheduler...\n");

    /* --- Activar proteccion de memoria --- */
    mpu_init();

    __asm volatile ("msr psp, %0" : : "r" (0));
    systick_init(1250000);

    while (1) {
        __asm volatile ("wfi");
    }
}
