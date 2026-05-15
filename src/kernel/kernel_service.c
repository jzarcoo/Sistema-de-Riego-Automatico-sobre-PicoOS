#include <stdint.h>
#include "syscalls.h"

/* Syscall ID for GPIO set operation*/
#define SYS_GPIO_SET 1
/* Syscall ID for GPIO get operation*/
#define SYS_GPIO_GET 2
/* Syscall ID for GPIO direction configuration */
#define SYS_GPIO_DIR 3
/* NEW: Syscall ID to terminate a task/process */
#define SYS_EXIT 4
/* NEW: Syscall IDs for semaphore operations */
#define SYS_SEM_WAIT 5 // Wait operation on a semaphore
#define SYS_SEM_POST 6 // Post operation on a semaphore
#define SYS_PUMP_ON 7
#define SYS_PUMP_OFF 8
#define SYS_READ_SOIL_SENSOR 9
#define SYS_LOG_EVENT 10 // DESACTIVADA
#define SYS_HEARTBEAT 11
#define SYS_SEM_INIT 12
#define SYS_ADC_INIT 13
#define SYS_PULLUP 14

// Declarations of kernel-level GPIO functions (defined in kernel_driver.c)
extern void k_gpio_set(uint32_t pin, uint32_t value);
extern int k_gpio_get(uint32_t pin);
extern void k_gpio_init(uint32_t pin, uint32_t output);
extern void k_gpio_pullup(uint32_t pin);
// Declaration of scheduler exit function (defined in scheduler.c)
extern void k_task_exit(void);

extern void kernel_pump_on(void);
extern void kernel_pump_off(void);
extern int kernel_read_soil_sensor(void);
extern void kernel_task_heartbeat(void);

extern void k_sem_init(semaphore_t *sem, int valor_inicial);
extern void k_sem_wait(semaphore_t *sem);
extern void k_sem_post(semaphore_t *sem);

extern void k_adc_init(void);

/*
 * @brief Kernel service handler for system calls.
 * @param svc_args: Pointer to the user stack frame.
 * @param syscall_id: The value of r7 passed by the assembler
 */
void kernel_service(uint32_t *svc_args, uint32_t syscall_id) {
    switch (syscall_id) {
        case SYS_GPIO_SET:
            // Set the GPIO value using the kernel function
            k_gpio_set(svc_args[0], svc_args[1]);
            break;

        case SYS_GPIO_GET:
            // Read the GPIO value using the kernel function
            int result = k_gpio_get(svc_args[0]);
            // Place the result back in r0 (svc_args[0]) to return to user space
            svc_args[0] = result; 
            break;

        case SYS_GPIO_DIR:
            // Validate before executing.
            // Example: prohibit touching pins 0 and 1 (UART) or pins > 28 (system)
            int pin = svc_args[0];
            if (pin < 2 || pin > 28) {
                svc_args[0] = -1; // Error: Permission denied (EACCES)
            } else {
                // If safe, call the driver
                k_gpio_init(pin, svc_args[1]);
                svc_args[0] = 0; // Success
            }
            break;

        case SYS_EXIT:
            // The task requested to terminate; delegate to the scheduler
            k_task_exit();
            break;
        
        case SYS_SEM_WAIT:
            // Wait on the semaphore 
            k_sem_wait((semaphore_t *)svc_args[0]);
            break;
        
        case SYS_SEM_POST:
            // Post to the semaphore 
            k_sem_post((semaphore_t *)svc_args[0]);
            break;

        case SYS_PUMP_ON:
            kernel_pump_on();
            break;

        case SYS_PUMP_OFF:
            kernel_pump_off();
            break;

        case SYS_READ_SOIL_SENSOR:
            svc_args[0] = kernel_read_soil_sensor();
            break;

        case SYS_HEARTBEAT:
            kernel_task_heartbeat();
            break;

        case SYS_SEM_INIT:
            k_sem_init((semaphore_t *)svc_args[0], svc_args[1]);
            break;

        case SYS_ADC_INIT:
            k_adc_init();
            break;

        case SYS_PULLUP:
            k_gpio_pullup(svc_args[0]);
            break;

        default:
            // Invalid syscall ID, return an error code (e.g., -1)
            svc_args[0] = -1; 
            break;
    }
}