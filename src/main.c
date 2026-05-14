#include <stdio.h>
#include <stdint.h>
#include "pico/stdlib.h"
#include "hardware/exception.h"

#include "user_app.h"

// #include "logger.h"
// #include "filesystem.h"

// Registros de control del SysTick (Timer de hardware integrado en ARM Cortex-M)
#define SYSTICK_BASE     0xE000E000
#define SYSTICK_CTRL (*(volatile uint32_t *)(SYSTICK_BASE + 0x10))
#define SYSTICK_LOAD (*(volatile uint32_t *)(SYSTICK_BASE + 0x14))
#define SYSTICK_VAL  (*(volatile uint32_t *)(SYSTICK_BASE + 0x18))

// Registrar los manejadores de Excepciones del Sistema Operativo
// SVC (ID 11) maneja las Syscalls solicitadas por las tareas
#define EXCEPTION_SVC     11
// PendSV (ID 14) maneja el Cambio de Contexto asíncrono
#define EXCEPTION_PENDSV  14

// Wrappers de Ensamblador (Manejadores de excepciones)
extern void wrapper_svc(void);
extern void isr_pendsv(void);

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
 * @brief Espera a que se establezca la conexión USB antes de continuar con la ejecución del sistema.
 */
void wait_for_usb_connection(void) {
    while (!stdio_usb_connected()) {
        printf("Esperando conexión USB...\n");
        sleep_ms(100);
    }
    printf("Conexión USB establecida.\n");
}

int main() {
    // Inicializar I/O Estándar
    stdio_init_all();
    while (!stdio_usb_connected()) {
        printf("Esperando conexión USB...\n");
        sleep_ms(100);
    }
    printf("Conexión USB establecida.\n");

    // Registrar manejadores exclusivos de excepciones del Kernel
    exception_set_exclusive_handler((enum exception_number)EXCEPTION_SVC, wrapper_svc);
    exception_set_exclusive_handler((enum exception_number)EXCEPTION_PENDSV, isr_pendsv);

    // Inicializar las tareas de usuario y registrarlas en el Scheduler
    user_app_init();

    // Inicializar el Process Stack Pointer (PSP) a 0.
    // Esto le avisa al ensamblador que iniciamos en modo Idle (sin tareas de usuario previas)
    __asm volatile ("msr psp, %0" : : "r" (0));

    // Configurar e iniciar SysTick
    systick_init(1250000); 

    // Bucle Idle (El sistema descansa hasta que SysTick lo despierte)
    while (1) {
        __asm volatile ("wfi"); // Wait For Interrupt (Detiene el reloj del CPU para ahorrar energía)
    }
}


// prueba filesystem
// int main() {

//     stdio_init_all();

//     sleep_ms(2000);

//     printf("FS TEST\n");

//     logger_init();

//     logger_write("Boot OK\n");

//     char buffer[512];

//     fs_read("irrig.log",
//             (uint8_t*)buffer,
//             sizeof(buffer));

//     printf("%s\n", buffer);

//     while (1) {
//         tight_loop_contents();
//     }
//}
