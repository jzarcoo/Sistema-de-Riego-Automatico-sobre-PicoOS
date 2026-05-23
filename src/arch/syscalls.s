.syntax unified
.cpu cortex-m0plus
.thumb

.equ SYS_GPIO_SET, 1
.equ SYS_GPIO_GET, 2
.equ SYS_EXIT,     4
.equ SYS_SEM_WAIT, 5
.equ SYS_SEM_POST, 6
.equ SYS_READ_SOIL_SENSOR,  9
.equ SYS_HEARTBEAT,    11
.equ SYS_SEM_INIT,     12
.equ SYS_REQUEST_IRRIGATION, 17
.equ SYS_SLEEP, 18
.equ SYS_PRINT, 19
.equ SYS_REQUEST_DISPLAY_UPDATE, 20
.equ SYS_DISPLAY_REFRESH, 21
.equ SYS_IRRIGATION_UPDATE, 22
.equ SYS_LOGGER_INIT, 23
.equ SYS_LOG_WRITE, 24
.equ SYS_LOG_FLUSH, 25

@ --- Function: void sys_gpio_set(int pin, int value) ---
.global sys_gpio_set
.type sys_gpio_set, %function
sys_gpio_set:
    mov r12, r7
    movs r7, #SYS_GPIO_SET
    svc #0
    mov r7, r12
    bx lr

@ --- Function: int sys_gpio_get(int pin) ---
.global sys_gpio_get
.type sys_gpio_get, %function
sys_gpio_get:
    mov r12, r7
    movs r7, #SYS_GPIO_GET
    svc #0
    mov r7, r12
    bx lr

@ --- Function: void sys_exit(void) ---
.global sys_exit
.type sys_exit, %function
sys_exit:
    mov r12, r7
    movs r7, #SYS_EXIT
    svc #0
    mov r7, r12
    bx lr

@ --- Function: void sys_sem_wait(kernel_semaphore_t *sem) ---
.global sys_sem_wait
.type sys_sem_wait, %function
sys_sem_wait:
    mov r12, r7
    movs r7, #SYS_SEM_WAIT
    svc #0
    mov r7, r12
    bx lr

@ --- Function: void sys_sem_post(kernel_semaphore_t *sem) ---
.global sys_sem_post
.type sys_sem_post, %function
sys_sem_post:
    mov r12, r7
    movs r7, #SYS_SEM_POST
    svc #0
    mov r7, r12
    bx lr

@ --- Function: int sys_read_soil_sensor(void) ---
.global sys_read_soil_sensor
.type sys_read_soil_sensor, %function
sys_read_soil_sensor:
    mov r12, r7
    movs r7, #SYS_READ_SOIL_SENSOR
    svc #0
    mov r7, r12
    bx lr

@ --- Function: void sys_heartbeat(void) ---
.global sys_heartbeat
.type sys_heartbeat, %function
sys_heartbeat:
    mov r12, r7
    movs r7, #SYS_HEARTBEAT
    svc #0
    mov r7, r12
    bx lr

@ --- Function: void sys_sem_init(kernel_semaphore_t *sem, int value) ---
.global sys_sem_init
.type sys_sem_init, %function
sys_sem_init:
    mov r12, r7
    movs r7, #SYS_SEM_INIT
    svc #0
    mov r7, r12
    bx lr

@ --- Function: void sys_request_irrigation(void) ---
.global sys_request_irrigation
.type sys_request_irrigation, %function
sys_request_irrigation:
    mov r12, r7
    movs r7, #SYS_REQUEST_IRRIGATION
    svc #0
    mov r7, r12
    bx lr

@ --- Function: void sys_sleep(uint32_t ms) ---
.global sys_sleep
.type sys_sleep, %function
sys_sleep:
    mov r12, r7
    movs r7, #SYS_SLEEP
    svc #0
    mov r7, r12
    bx lr

@ --- Function: void sys_print(const char* str) ---
.global sys_print
.type sys_print, %function
sys_print:
    mov r12, r7
    movs r7, #SYS_PRINT
    svc #0
    mov r7, r12
    bx lr

@ --- Function: void sys_display_write(int row, const char* text) ---
.global sys_display_write
.type sys_display_write, %function
sys_display_write:
    mov r12, r7
    movs r7, #SYS_REQUEST_DISPLAY_UPDATE
    svc #0
    mov r7, r12
    bx lr

@ --- Function: void sys_display_flush(void) ---
.global sys_display_flush
.type sys_display_flush, %function
sys_display_flush:
    mov r12, r7
    movs r7, #SYS_DISPLAY_REFRESH
    svc #0
    mov r7, r12
    bx lr

@ --- Function: void sys_irrigation_update(void) ---
.global sys_irrigation_update
.type sys_irrigation_update, %function
sys_irrigation_update:
    mov r12, r7
    movs r7, #SYS_IRRIGATION_UPDATE
    svc #0
    mov r7, r12
    bx lr

@ --- Function: void sys_logger_init(void) ---
.global sys_logger_init
.type sys_logger_init, %function
sys_logger_init:
    mov r12, r7
    movs r7, #SYS_LOGGER_INIT
    svc #0
    mov r7, r12
    bx lr

@ --- Function: void sys_log_write(const char* text) ---
.global sys_log_write
.type sys_log_write, %function
sys_log_write:
    mov r12, r7
    movs r7, #SYS_LOG_WRITE
    svc #0
    mov r7, r12
    bx lr

@ --- Function: void sys_log_flush(void) ---
.global sys_log_flush
.type sys_log_flush, %function
sys_log_flush:
    mov r12, r7
    movs r7, #SYS_LOG_FLUSH
    svc #0
    mov r7, r12
    bx lr
