#include <stdint.h>
#include "kernel_drivers.h"
#include "kernel_events.h"
#include "irrigation_manager.h"
#include "semaphore.h"
#include "scheduler.h"
#include "watchdog_supervisor.h"
#include "message_queue.h"

#define SYS_GPIO_SET 1
#define SYS_GPIO_GET 2
#define SYS_GPIO_DIR 3
#define SYS_EXIT 4
#define SYS_SEM_WAIT 5
#define SYS_SEM_POST 6
#define SYS_PUMP_ON 7
#define SYS_PUMP_OFF 8
#define SYS_READ_SOIL_SENSOR 9
#define SYS_LOG_EVENT 10
#define SYS_HEARTBEAT 11
#define SYS_SEM_INIT 12
#define SYS_ADC_INIT 13
#define SYS_PULLUP 14
#define SYS_GPIO_IRQ_INIT 15
#define SYS_MANUAL_TRIGGER_EVENT_INIT 16
#define SYS_REQUEST_IRRIGATION 17

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

        case SYS_GPIO_GET: {
            int result = k_gpio_get(svc_args[0]);
            svc_args[0] = result;
            break;
        }

        case SYS_GPIO_DIR: {
            int pin = svc_args[0];
            if (pin < 2 || pin > 28) {
                svc_args[0] = -1;
            } else {
                k_gpio_init(pin, svc_args[1]);
                svc_args[0] = 0;
            }
            break;
        }

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

        case SYS_GPIO_IRQ_INIT:
            k_gpio_irq_init(svc_args[0]);
            break;

        case SYS_MANUAL_TRIGGER_EVENT_INIT:
            k_manual_trigger_event_init((message_queue_t *)svc_args[0]);
            break;

        case SYS_REQUEST_IRRIGATION:
            irrigation_manager_request_water();
            break;

        default:
            // Invalid syscall ID, return an error code (e.g., -1)
            svc_args[0] = -1; 
            break;
    }
}