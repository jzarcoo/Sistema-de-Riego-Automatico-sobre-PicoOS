#include <stdint.h>
#include <stdio.h>
#include "pico/multicore.h"
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
#define SYS_GPIO_IRQ_REGISTER 15
#define SYS_REQUEST_IRRIGATION 17
#define SYS_SLEEP 18
#define SYS_PRINT 19

void kernel_service(uint32_t *svc_args, uint32_t syscall_id) {
    switch (syscall_id) {
        case SYS_GPIO_SET:
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
            k_task_exit();
            break;

        case SYS_SEM_WAIT:
            k_sem_wait((kernel_semaphore_t *)svc_args[0]);
            break;

        case SYS_SEM_POST:
            k_sem_post((kernel_semaphore_t *)svc_args[0]);
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
            k_sem_init((kernel_semaphore_t *)svc_args[0], svc_args[1]);
            break;

        case SYS_ADC_INIT:
            k_adc_init();
            break;

        case SYS_PULLUP:
            k_gpio_pullup(svc_args[0]);
            break;

        case SYS_GPIO_IRQ_REGISTER:
            k_gpio_event_register(
                svc_args[0],
                svc_args[1],
                (message_queue_t *)svc_args[2],
                (message_type_t)svc_args[3]
            );
            
            break;

        case SYS_REQUEST_IRRIGATION:
            irrigation_manager_request_water();
            break;

        case SYS_SLEEP:
            k_task_sleep(svc_args[0]);
            break;

        case SYS_PRINT:
            printf("%s", (const char *)svc_args[0]);
            break;

        default:
            svc_args[0] = -1;
            break;
    }
}
