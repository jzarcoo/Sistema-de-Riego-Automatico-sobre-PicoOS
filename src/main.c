#include <stdio.h>
#include <stdint.h>
#include "pico/stdlib.h"
#include "hardware/exception.h"

#include "scheduler.h"
#include "irrigation_manager.h"

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

#define EXCEPTION_SVC     11
#define EXCEPTION_PENDSV  14

extern void wrapper_svc(void);
extern void isr_pendsv(void);

void systick_init(__uint32_t ticks) {
    SYSTICK_LOAD = ticks - 1;
    SYSTICK_VAL = 0;
    SYSTICK_CTRL = 0x07;
}

int main() {
    stdio_init_all();
    while (!stdio_usb_connected()) {
        printf("Esperando conexion USB...\n");
        sleep_ms(100);
    }
    printf("Conexion USB establecida.\n");

    exception_set_exclusive_handler((enum exception_number)EXCEPTION_SVC, wrapper_svc);
    exception_set_exclusive_handler((enum exception_number)EXCEPTION_PENDSV, isr_pendsv);

    // Kernel registra las tareas de usuario en el scheduler
    task_create(IRRIGATION_TASK_ID, irrigation_task);
    task_create(SENSOR_TASK_ID, sensor_task);
    task_create(TRIGGER_TASK_ID, trigger_task);
    task_create(LOGGER_TASK_ID, logger_task);
    task_create(DISPLAY_TASK_ID, display_task);

    __asm volatile ("msr psp, %0" : : "r" (0));

    systick_init(1250000);

    while (1) {
        irrigation_manager_update();
        __asm volatile ("wfi");
    }
}

