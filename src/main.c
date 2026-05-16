#include <stdio.h>
#include <stdint.h>
#include "pico/stdlib.h"
#include "hardware/exception.h"

#include "scheduler.h"
#include "irrigation_manager.h"
#include "user_app.h"
#include "syscalls.h"

// Prototipos de las tareas de usuario (definidas en src/user/tasks/)
extern void irrigation_task(void);
extern void sensor_task(void);
extern void trigger_task(void);
extern void logger_task(void);
extern void display_task(void);

#define IRRIGATION_TASK_ID  0
#define SENSOR_TASK_ID      1
#define TRIGGER_TASK_ID     2
#define LOGGER_TASK_ID      3
#define DISPLAY_TASK_ID     4

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

// Registrar los manejadores de Excepciones del Sistema Operativo
// SVC (ID 11) maneja las Syscalls solicitadas por las tareas
#define EXCEPTION_SVC     11
#define EXCEPTION_PENDSV  14
// HardFault (ID 3) maneja errores críticos del sistema
#define EXCEPTION_HARDFAULT  3

extern void wrapper_svc(void);
extern void isr_pendsv(void);

extern volatile uint32_t kernel_ticks;

// Contador de hard faults 
volatile uint32_t fault_count = 0;
// Última dirección de programa que causó un hard fault
volatile uint32_t last_fault_pc = 0;

/**
 * @brief Configura e inicializa el timer SysTick para generar interrupciones periódicas.
 * @param ticks Número de ciclos del reloj para generar una interrupción (ej. 1250000 para 10ms con un reloj de 125MHz).
 */
void systick_init(__uint32_t ticks) {
    SYSTICK_LOAD = ticks - 1;
    SYSTICK_VAL = 0;          
    SYSTICK_CTRL = 0x07;      
}

/**
 * @brief Registra un incidente de hard fault.
 * @param fault_pc Dirección de programa donde ocurrió el hard fault.
 */
void log_incident(uint32_t fault_pc) {
    fault_count++;
    last_fault_pc = fault_pc;
    uint32_t timestamp = kernel_ticks; 
    printf("\n[!] =============================================\n");
    printf("[HARD FAULT] Ocurrió un hard fault en la dirección 0x%08X\n", fault_pc);
    printf("[HARD FAULT] Contador de fallos: %u\n", fault_count);
    printf("[HARD FAULT] Timestamp: %u\n", timestamp);
    printf("\n[!] =============================================\n");
}

/**
 * @brief C-part of the fault handler.
 */
void HardFault_Handler_C(uint32_t *stack_frame) {
    log_incident(stack_frame[6]);
    sys_exit(); // Terminar la tarea actual que causó el hard fault
}

/**
 * @brief Assembly entry for HardFault 
 */
void __attribute__((naked)) HardFault_Handler(void) {
    __asm volatile(
        "movs r1, #4           \n"
        "mov  r0, lr           \n"
        "tst  r0, r1           \n"
        "beq  use_msp          \n"
        // usa process stack pointer (PSP)
        "mrs  r0, psp          \n"
        "b    call_handler     \n"
        // usa main stack pointer (MSP)
        "use_msp:              \n"
        "mrs  r0, msp          \n"
        "call_handler:         \n"
        "b HardFault_Handler_C \n"
    );
}

void mpu_protect_peripherials(void) {
    MPU_RNR = 1;                              
    MPU_RBAR = 0x40000000; // Perifericos: GPIO, UART, ADC, etc.
    MPU_RASR = (1 << 28) | // no codigo
               (1<<24) |   // Kernel puede leer/escribir. Tareas de usuario causarán HardFault
               (1<< 18) | // sharable bit
               (28 << 1) |        // Protegemos 512MB desde la base
               (1 << 0);             // Activamos esta regla
               
               
    MPU_RNR = 2;                          
    MPU_RBAR = 0xD0000000; // SIO
    MPU_RASR = (1 << 28) |                       
               (1<<24) |   // Solo Kernel puede tocar los GPIOs rápidos
               (1<< 18) |            
               (27 << 1) |        // Protegemos 256MB
               (1 << 0);            
}

void mpu_init(void) {
    __asm volatile("dmb"); // Data Memory Barrier
    
    MPU_CTRL = 0; // Deshabilitar MPU para configurar

    mpu_protect_peripherials();

    // Permite que el código privilegiado use el mapa de memoria por defecto en direcciones no cubiertas por regiones MPU.
    MPU_CTRL = (1 << 0) | (1 << 2); 

    __asm volatile("dsb"); // Data Synchronization Barrier
    __asm volatile("isb"); // Instruction Synchronization Barrier
}

static inline void drop_privileges(void) {
    __asm volatile (
        "movs r0, #3     \n"
        "msr control, r0 \n"
        "isb             \n"
        :
        :
        : "r0", "memory"
    );
}

int main() {
    stdio_init_all();
    // Espera a que se establezca la conexión USB antes de continuar con la ejecución del sistema.
    while (!stdio_usb_connected()) {
        printf("Esperando conexion USB...\n");
        sleep_ms(100);
    }
    printf("Conexion USB establecida.\n");

    exception_set_exclusive_handler((enum exception_number)EXCEPTION_SVC, wrapper_svc);
    exception_set_exclusive_handler((enum exception_number)EXCEPTION_PENDSV, isr_pendsv);
    exception_set_exclusive_handler((enum exception_number)EXCEPTION_HARDFAULT, HardFault_Handler); 

    mpu_init();

    // Kernel registra las tareas de usuario en el scheduler
    task_create(IRRIGATION_TASK_ID, irrigation_task);
    task_create(SENSOR_TASK_ID, sensor_task);
    task_create(TRIGGER_TASK_ID, trigger_task);
    task_create(LOGGER_TASK_ID, logger_task);
    task_create(DISPLAY_TASK_ID, display_task);

    __asm volatile ("msr psp, %0" : : "r" (0));

    systick_init(1250000);

    // Modo no privilegiado
    drop_privileges();

    while (1) {
        irrigation_manager_update();
        __asm volatile ("wfi");
    }
}

