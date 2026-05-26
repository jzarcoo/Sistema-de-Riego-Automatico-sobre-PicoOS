/**
 * @file kernel_service.c
 * @brief Despachador de syscalls (Supervisor Call handler).
 *
 * Las tareas en modo no privilegiado invocan SVC con un ID;
 * el handler extrae argumentos del stack frame y despacha aqui.
 */

#include <stdint.h>
#include <stdio.h>
#include "kernel_drivers.h"
#include "irrigation_manager.h"
#include "display_manager.h"
#include "semaphore.h"
#include "scheduler.h"
#include "watchdog_supervisor.h"
#include "logger.h"
#include "log_memory.h"

#define SYS_GPIO_SET 1
#define SYS_GPIO_GET 2
#define SYS_EXIT 4
#define SYS_SEM_WAIT 5
#define SYS_SEM_POST 6
#define SYS_READ_SOIL_SENSOR 9
#define SYS_HEARTBEAT 11
#define SYS_SEM_INIT 12
#define SYS_REQUEST_IRRIGATION 17
#define SYS_SLEEP 18
#define SYS_PRINT 19
#define SYS_REQUEST_DISPLAY_UPDATE 20
#define SYS_DISPLAY_REFRESH 21
#define SYS_IRRIGATION_UPDATE 22
#define SYS_LOGGER_INIT 23
#define SYS_LOG_WRITE 24
#define SYS_LOG_FLUSH 25

/** @brief Despachador de syscalls. Recibe args del stack frame y ejecuta en modo privilegiado. */
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

        case SYS_EXIT:
            k_task_exit();
            break;

        case SYS_SEM_WAIT:
            k_sem_wait((kernel_semaphore_t *)svc_args[0]);
            break;

        case SYS_SEM_POST:
            k_sem_post((kernel_semaphore_t *)svc_args[0]);
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

        case SYS_REQUEST_IRRIGATION:
            irrigation_manager_request_water();
            break;

        case SYS_SLEEP:
            k_task_sleep(svc_args[0]);
            break;

        case SYS_PRINT:
            printf("%s", (const char *)svc_args[0]);
            break;

        case SYS_REQUEST_DISPLAY_UPDATE:
            display_manager_set_row((int)svc_args[0], (const char *)svc_args[1]);
            break;

        case SYS_DISPLAY_REFRESH:
            display_manager_refresh();
            break;

        case SYS_IRRIGATION_UPDATE:
            irrigation_manager_update();
            break;

        case SYS_LOGGER_INIT:
            logger_init();
            log_memory_init();
            break;

        case SYS_LOG_WRITE:
            log_cache_write((const char *)svc_args[0]);
            break;

        case SYS_LOG_FLUSH:
            log_flush_all();
            break;

        default:
            svc_args[0] = -1;
            break;
    }
}
